#include <gtest/gtest.h>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include "Piwik.h"

namespace {
class CommaNumbers : public std::numpunct<char>
{
protected:
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '.'; }
    std::string do_grouping() const override { return "\3"; }
};

class ScopedCommaLocale
{
public:
    ScopedCommaLocale() : previous(std::locale())
    {
        std::locale::global(std::locale(std::locale::classic(), new CommaNumbers));
    }
    ~ScopedCommaLocale() { std::locale::global(previous); }
private:
    std::locale previous;
};

PiwikState NumericState()
{
    PiwikState state;
    state.SiteId = 1234;
    state.Random = 1234567;
    state.TrackedPath = _T("/startup");
    return state;
}
}

TEST(Numbers, UrlNumbersIgnoreApplicationLocale)
{
    ScopedCommaLocale locale;
    auto state = NumericState();
    state.EventValue = 1250.5;
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_NE(std::string::npos, query.find("?idsite=1234&"));
    EXPECT_NE(std::string::npos, query.find("&rand=1234567&"));
    EXPECT_NE(std::string::npos, query.find("&e_v=1250.5"));
}

TEST(Numbers, BulkNumbersIgnoreApplicationLocale)
{
    ScopedCommaLocale locale;
    auto state = NumericState();
    state.EventValue = 1250.5;
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_JSON).find("&e_v=1250.5"));
}

TEST(Numbers, NonFiniteEventCannotBeSerialized)
{
    auto state = NumericState();
    state.EventValue = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_URL), std::invalid_argument);
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_JSON), std::invalid_argument);
}

TEST(Numbers, NonFiniteRevenueCannotBeSerialized)
{
    auto state = NumericState();
    state.Revenue = std::numeric_limits<float>::infinity();
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_URL), std::invalid_argument);
}

TEST(Numbers, ExplicitZeroIsDifferentFromAnAbsentValue)
{
    auto state = NumericState();
    EXPECT_EQ(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v="));
    state.SetEventValue(0.0);
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v=0"));
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_JSON).find("&e_v=0"));
    state.ClearEventValue();
    EXPECT_EQ(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v="));
}

TEST(Numbers, LegacyDirectZeroAssignmentRemainsAbsent)
{
    auto state = NumericState();
    state.EventValue = 0;
    EXPECT_FALSE(state.HasEventValue);
    EXPECT_EQ(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v="));
}

TEST(Numbers, LegacyDirectNonzeroAssignmentIsStillSerialized)
{
    auto state = NumericState();
    state.EventValue = -1;
    EXPECT_TRUE(state.HasFiniteNumbers());
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v=-1"));
}

TEST(Numbers, SerializationDoesNotChangeTheApplicationsLocale)
{
    ScopedCommaLocale locale;
    auto state = NumericState();
    state.SetEventValue(1250.5);
    state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_EQ(',', std::use_facet<std::numpunct<char>>(std::locale()).decimal_point());
}

TEST(Numbers, PositiveExponentIsEscapedInBothQueryFormats)
{
    auto state = NumericState();
    state.SetEventValue(1e100);
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_URL).find("&e_v=1e%2B100"));
    EXPECT_NE(std::string::npos, state.Serialize(PIWIK_FORMAT_JSON).find("&e_v=1e%2B100"));
}

class NumericRoundTripTest : public ::testing::TestWithParam<double> {};

TEST_P(NumericRoundTripTest, PreservesDoublePrecision)
{
    auto state = NumericState();
    state.SetEventValue(GetParam());
    for (auto format : {PIWIK_FORMAT_URL, PIWIK_FORMAT_JSON})
    {
        const auto query = state.Serialize(format);
        const auto position = query.find("&e_v=");
        ASSERT_NE(std::string::npos, position);
        auto value = query.substr(position + 5);
        const auto end = value.find('&');
        if (end != std::string::npos) value.resize(end);
        const auto plus = value.find("%2B");
        if (plus != std::string::npos) value.replace(plus, 3, "+");
        std::istringstream parser(value);
        parser.imbue(std::locale::classic());
        double actual = 0;
        parser >> actual;
        ASSERT_FALSE(parser.fail()) << value;
        EXPECT_EQ(GetParam(), actual) << value;
    }
}

INSTANTIATE_TEST_SUITE_P(FiniteValues, NumericRoundTripTest, ::testing::Values(
    0.0, 0.1, 1250.1234567890123, -1.25, 1e100, 1e-100,
    (std::numeric_limits<double>::max)(), (std::numeric_limits<double>::min)()));

class NonFiniteTest : public ::testing::TestWithParam<double> {};

TEST_P(NonFiniteTest, InvalidNumbersAreRejectedInBothFormats)
{
    auto state = NumericState();
    state.SetEventValue(GetParam());
    EXPECT_FALSE(state.HasFiniteNumbers());
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_URL), std::invalid_argument);
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_JSON), std::invalid_argument);
    state.ClearEventValue();
    state.Revenue = static_cast<float>(GetParam());
    EXPECT_FALSE(state.HasFiniteNumbers());
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_URL), std::invalid_argument);
    EXPECT_THROW(state.Serialize(PIWIK_FORMAT_JSON), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(InvalidValues, NonFiniteTest, ::testing::Values(
    std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
    -std::numeric_limits<double>::infinity()));

TEST(Numbers, LegacyGenerationTimeKeepsItsOldWireField)
{
    auto state = NumericState();
    state.AmountOfTime = 1250;
    const auto query = state.Serialize(PIWIK_FORMAT_URL);
    EXPECT_NE(std::string::npos, query.find("&gt_ms=1250"));
    EXPECT_EQ(std::string::npos, query.find("&e_v="));
    EXPECT_EQ(std::string::npos, query.find("&pf_srv="));
}
