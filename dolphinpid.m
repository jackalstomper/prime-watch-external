#include <Cocoa/Cocoa.h>
#include "common.h"

int dolphinPid() {
//  NSString *output = [NSString string];
  for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications]) {
    NSString *procName = [[app executableURL] lastPathComponent];
//    output = [output stringByAppendingFormat:@"%@\n", procName];
//    NSLog(@"%@\n", procName);
    if ([procName isEqualToString:@"Dolphin"]) {
      return [app processIdentifier];
    }
  }
//  [output writeToFile:@"./appslist.txt"
//           atomically:YES
//             encoding:NSUTF8StringEncoding
//                error:NULL];
  return -1;
}