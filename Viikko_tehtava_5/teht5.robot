*** Settings ***
Library   String
Library   SerialLibrary

*** Variables ***
${com}           COM3
${baud}          115200
${board}         nrf
${encoding}      ascii

# Testisyötteet
${valid_time}    000120X
${valid_result}  80X
${invalid_time}	 001067X
${invalid_result}	-1X

*** Test Cases ***
connect serial
    Log To Console  Connecting to ${board}
    Add Port    ${com}    baudrate=${baud}    encoding=${encoding}
    Port Should Be Open    ${com}
    Reset Input Buffer
    Reset Output Buffer

valid time string
    Log To Console  Testing valid time string ${valid_time}
    Write Data    ${valid_time}    encoding=${encoding}
    ${resp}=      Read Until    terminator=58    encoding=${encoding}  # terminator=ASCII 58 -> 'X'
    Log To Console  Received: ${resp}
    Should Be Equal As Strings    ${resp}    ${valid_result}
    Log To Console  Valid time string test passed

invalid time string
    Log To Console  Testing invalid time string ${invalid_time}
    Write Data    ${invalid_time}    encoding=${encoding}
    ${resp}=      Read Until    terminator=58    encoding=${encoding}
    Log To Console  Received: ${resp}
    Should Be Equal As Strings    ${resp}    ${invalid_result}
    Log To Console  Invalid time string test passed

disconnect serial
    Log To Console  Disconnecting ${board}
    [Teardown]    Delete Port    ${com}
