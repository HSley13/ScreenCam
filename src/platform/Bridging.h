#import <Foundation/Foundation.h>

#include <string_view>

/// Converts a C++ UTF-8 string (std::string or std::string_view) into an NSString.
/// The core returns plain C++ strings so it stays free of Apple headers; this is the one
/// place where we cross from the C++ world into the Objective-C world for text.
inline NSString *SCString(std::string_view text) {
    NSString *result = [[NSString alloc] initWithBytes:text.data()
                                                length:text.size()
                                              encoding:NSUTF8StringEncoding];
    return (result != nil) ? result : @"";
}
