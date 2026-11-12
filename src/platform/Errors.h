#import <Foundation/Foundation.h>

/// Builds an NSError whose localizedDescription is `message`, ready to show in the status label.
inline NSError *SCMakeError(NSString *message) {
    return [NSError errorWithDomain:@"com.hsley13.screencam"
                               code:-1
                           userInfo:@{NSLocalizedDescriptionKey : message}];
}
