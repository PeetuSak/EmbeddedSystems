#include <gtest/gtest.h>
#include "../TimeParser.h"

// Test suite: TimeParserTest
TEST(TimeParserTest, TestCaseCorrectTime) {

 

    // Test with correct time string
    char time_test[] = "000005";
    EXPECT_EQ(time_parse(time_test),5);

    char time_test2[] = "000105";
    EXPECT_EQ(time_parse(time_test2),65);

    char time_test3[] = "235959";
    EXPECT_EQ(time_parse(time_test3),65);
}    

TEST(TimeParserTest, TestCaseIncorrectTime) {

    char time_test[] = "000077";
    EXPECT_EQ(time_parse(time_test), TIME_VALUE_ERROR);

    char time_test2[] = "008800";
    EXPECT_EQ(time_parse(time_test2), TIME_VALUE_ERROR);

    char time_test3[] = "246000";
    EXPECT_EQ(time_parse(time_test3), TIME_VALUE_ERROR);
}    

TEST(TimeParserTest, TestStringLeng) {

    char time_test[] = "00005";
    EXPECT_EQ(time_parse(time_test), TIME_LEN_ERROR);

    char time_test2[] = "0000055";
    EXPECT_EQ(time_parse(time_test2), TIME_LEN_ERROR);
}
// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
