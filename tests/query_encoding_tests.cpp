#include <gtest/gtest.h>
#include <map>
#include <stdexcept>
#include "Piwik.h"

namespace {
// Model the server's form-query decoding independently of the SDK encoder.
int Hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    throw std::invalid_argument("Invalid query escape");
}

std::string Decode(const std::string& value)
{
    std::string result;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '%')
        {
            if (i + 2 >= value.size()) throw std::invalid_argument("Incomplete query escape");
            const auto decoded = static_cast<char>(16 * Hex(value[i + 1]) + Hex(value[i + 2]));
            result += decoded;
            i += 2;
        }
        else result += value[i] == '+' ? ' ' : value[i];
    }
    return result;
}

std::map<std::string, std::string> ParseQuery(const std::string& query)
{
    std::map<std::string, std::string> values;
    size_t pos = query.at(0) == '?' ? 1 : 0;
    while (pos < query.size())
    {
        const auto end = query.find('&', pos);
        const auto parameter = query.substr(pos, end == std::string::npos ? end : end - pos);
        const auto separator = parameter.find('=');
        if (separator == std::string::npos) throw std::invalid_argument("Missing query assignment");
        const auto inserted = values.emplace(Decode(parameter.substr(0, separator)), Decode(parameter.substr(separator + 1)));
        if (!inserted.second) throw std::invalid_argument("Duplicate query parameter");
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return values;
}

PiwikState State()
{
    PiwikState state;
    state.SiteId = 7;
    state.TrackedPath = _T("/event");
    state.UserAgent.clear();
    state.Random = 42;
    return state;
}

struct StringField { const char* key; TSTRING PiwikState::* member; };
class QueryStringField : public ::testing::TestWithParam<StringField> {};
}

TEST_P(QueryStringField, GetAndBulkRoundTripWithoutInjectingParameters)
{
    auto state = State();
    const auto field = GetParam();
    state.*field.member = _T(" a&rec=0&e_v=999+b=%2F/#\"\\\r\n\t");
    const std::string expected = " a&rec=0&e_v=999+b=%2F/#\"\\\r\n\t";
    const auto get = ParseQuery(state.Serialize(PIWIK_FORMAT_URL));
    const auto bulk = ParseQuery(state.Serialize(PIWIK_FORMAT_JSON));
    EXPECT_EQ(expected, get.at(field.key));
    EXPECT_EQ(expected, bulk.at(field.key));
    EXPECT_EQ("1", bulk.at("rec"));
    EXPECT_EQ(0u, bulk.count("e_v"));
    EXPECT_EQ(get, bulk);
}

INSTANTIATE_TEST_SUITE_P(AllStringFields, QueryStringField, ::testing::Values(
    StringField{"url", &PiwikState::TrackedPath},
    StringField{"action_name", &PiwikState::TrackedAction},
    StringField{"uid", &PiwikState::UserId},
    StringField{"_id", &PiwikState::VisitorId},
    StringField{"ua", &PiwikState::UserAgent},
    StringField{"lang", &PiwikState::Language},
    StringField{"res", &PiwikState::ScreenRes},
    StringField{"e_c", &PiwikState::EventCategory},
    StringField{"e_a", &PiwikState::EventAction},
    StringField{"e_n", &PiwikState::EventName},
    StringField{"link", &PiwikState::OutLink},
    StringField{"c_n", &PiwikState::ContentName},
    StringField{"c_p", &PiwikState::ContentPiece},
    StringField{"c_t", &PiwikState::ContentTarget},
    StringField{"c_i", &PiwikState::ContentInteraction}),
    [](const ::testing::TestParamInfo<StringField>& info) { return info.param.key; });

TEST(QueryEncoding, UnicodeAndLiteralPercentEscapesAreDecodedExactlyOnce)
{
#ifdef UNICODE
    auto state = State();
    state.EventName = L"\u05E9\u00E9\U0001F680+%26";
    for (auto format : {PIWIK_FORMAT_URL, PIWIK_FORMAT_JSON})
        EXPECT_EQ("\xD7\xA9\xC3\xA9\xF0\x9F\x9A\x80+%26", ParseQuery(state.Serialize(format)).at("e_n"));
#else
    GTEST_SKIP() << "Unicode API test";
#endif
}

TEST(QueryEncoding, CustomVariablesAreJsonEscapedThenUrlEncodedWithOriginalSlots)
{
    auto state = State();
    state.UserVariables.Items[0].Set(_T("name\"\\\n"), _T("value&=+%\"\\\n"));
    state.UserVariables.Items[3].Set(_T("four"), _T("x/y"));
    state.ScreenVariables = state.UserVariables;
    const std::string expected = R"({"1":["name\"\\\n","value&=+%\"\\\n"],"4":["four","x\/y"]})";
    for (auto format : {PIWIK_FORMAT_URL, PIWIK_FORMAT_JSON})
    {
        const auto query = ParseQuery(state.Serialize(format));
        EXPECT_EQ(expected, query.at("_cvar"));
        EXPECT_EQ(expected, query.at("cvar"));
    }
    EXPECT_EQ(state.Serialize(PIWIK_FORMAT_URL), state.Serialize(PIWIK_FORMAT_JSON));
}

TEST(QueryEncoding, UnicodeCustomVariablesSurviveBothFormats)
{
#ifdef UNICODE
    auto state = State();
    state.ScreenVariables.Items[1].Set(L"\u05E9", L"\u00E9+&");
    for (auto format : {PIWIK_FORMAT_URL, PIWIK_FORMAT_JSON})
        EXPECT_EQ("{\"2\":[\"\xD7\xA9\",\"\xC3\xA9+&\"]}", ParseQuery(state.Serialize(format)).at("cvar"));
#else
    GTEST_SKIP() << "Unicode API test";
#endif
}

TEST(QueryEncoding, EveryByteRoundTripsThroughThePercentEncoder)
{
    std::string input;
    for (int value = 0; value <= 255; ++value) input += static_cast<char>(value);
    const auto encoded = PercentEncode(input);
    EXPECT_EQ(input, Decode(encoded));
    // The percent encoder's output must be safe within an outer JSON string.
    for (unsigned char value : encoded)
    {
        EXPECT_GE(value, 0x20);
        EXPECT_LT(value, 0x7F);
        EXPECT_NE('"', value);
        EXPECT_NE('\\', value);
    }
}
