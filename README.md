# piwik-sdk-wincpp

## Piwik C++ tracking SDK for Windows Desktop

### Design

This SDK makes possible for a Windows Desktop application to access a Piwik web server in order to track GUI events and generate statistical data about its usage. All kind of events can be tracked as standardly supported by the Piwik API, although some of them must be interpreted somehow differently in the context of a desktop application.

The main focus of this implementation has been to provide a generic component able to be easily integrated into any kind of Windows program on all platforms starting at Windows XP. No external libraries have been used so that no dependencies are generated for the hosting application, and any C++ compiler will be able to handle the code because only basic language constructs and STL runtime components have been taken into account, although it will certainly benefit from the extended optimization capabilities of modern compilers.

Reducing the footprint of the library to the absolute minimum has been another key objective, so that it can be used intensively without causing a noticeable overhead in resource consumption. Thanks to lightweight objects and direct execution paths tracking can be done under good network conditions without compromising the overall performance of the application.

### Basic usage

To integrate the Piwik SDK into a C++ application it is enough to include the header file "Piwik.h" provided in the include folder of the distribution and to link the executable against the appropriate edition of the library PiwikClient.lib which can be built either to use ANSI or UNICODE strings. 

After instantiating the PiwikClient object, which is the main component providing tracking facilities to the application, a set of properties can be assigned to it in order to define the appropriate endpoints and customize its behavior. Different operation modes are selectable like using synchronous or asynchronous dispatching with regulable time frames, sending HTTP requests with GET or POST methods or using encrypted secure connections. A stream pointer can also be passed to the tracker to activate logging facilities. 

Direct entry points are provided to the hosting application to track standard targets in a convenient way: user events, screen views, goals, display or content interactions can be tracked by simply supplying the most relevant parameters for each one of them. For more advanced usages, a low-level call is also provided in which the whole set of trackable values is made accessible to the calling function thus allowing to track complex or non-standard situations. At any time pending events can be flushed to the server if required and tracking can be finished by simply deleting the PiwikClient instance.

### Interface

To get a detailed insight into the interface of the library take a look at the [Reference Documentation](doc/Reference.md).

