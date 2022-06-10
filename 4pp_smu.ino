#include <Wire.h>
#include <Adafruit_ADS1X15.h>


#define MCP4725_ADDR 96

#define DIR1 2
#define DIR2 3
#define RELAY_1k 8
#define RELAY_10k 7
#define RELAY_100k 6
#define RELAY_1M 5
#define RELAY_10M 4

// these values are chosen such that the DAC output voltage is always < 1.0 V
#define MAX_CURRENT_1k 0.001
#define MAX_CURRENT_10k 0.0001
#define MAX_CURRENT_100k 0.00001
#define MAX_CURRENT_1M 0.000001
#define MAX_CURRENT_10M 0.0000001

//const float currentFactor = 2.0
//const float initialCurrent = 0.00000001 // 10 na
//const float voltageMinMeasure = 0.2

#define CURRENT_FACTOR 2.0
#define INITIAL_CURRENT 0.00000001
#define VOLTAGE_MIN_MEASURE 0.2
#define INAMP_GAIN 405.0 //AD627 gain = 5 + (200kOhm/Rg)
#define ADC_GAIN GAIN_TWO //ads1115 -> +/- 2.048V
//#define VOLTS_PER_BIT 0.0000625F 

#define NUM_SAMPLES 100
#define SAMPLE_INTERVAL 10 //ms

#define DAC_VREF 3.3
#define DELAY_SWITCH 500 //ms

double current = INITIAL_CURRENT;
double VOLTS_PER_BIT = 0.0000625;
Adafruit_ADS1115 ads1115;

// see Serial Input Basics by Robin2: https://forum.arduino.cc/t/serial-input-basics-updated/382007/2
const int maxChars = 32; //max number of chars when reading from serial into cstring. anything larger rewrites last char of cstring (?)
const int maxTokens = 10; //max number of "words" or "tokens" separated by spaces (e.g. "mv samx 1" is 3 words or 3 tokens). print error if exceeded
char incomingChars[maxChars]; //incoming chars until newline terminator
char incomingParams[maxTokens][maxChars]; //incoming chars parsed into cstrings
char tempChars[maxChars];
bool newSerialData = false; // whether there is new data parsed from serial that needs to be processed
int numTokens = 0; //number of tokens or params

char charACK[] = "ACK"; //"\x06";
char charNAK[] = "NAK";
char charSUCC[] = "SUCC";
char charFAIL[] = "FAIL";
char charWARN[] = "WARN";

bool isDebugging = true;

void setup() {
    // unpower all relays
    pinMode(RELAY_1k, OUTPUT);
    digitalWrite(RELAY_1k, LOW);
    pinMode(RELAY_10k, OUTPUT);
    digitalWrite(RELAY_10k, LOW);
    pinMode(RELAY_100k, OUTPUT);
    digitalWrite(RELAY_100k, LOW);
    pinMode(RELAY_1M, OUTPUT);
    digitalWrite(RELAY_1M, LOW);
    pinMode(RELAY_10M, OUTPUT);
    digitalWrite(RELAY_10M, LOW);
    pinMode(DIR1, OUTPUT);
    digitalWrite(DIR1, LOW);
    pinMode(DIR2, OUTPUT);
    digitalWrite(DIR2, LOW);
    
    Wire.begin();
    ads1115.begin();
    ads1115.setGain(ADC_GAIN);
    Serial.begin(115200);
}

void loop() {
    readNextSerial();
    if (newSerialData == true)
    {
        if (isDebugging)
        {
            Serial.print(">>");
            Serial.println(incomingChars);
        }

        strcpy(tempChars, incomingChars);
        parseTokens();
        goToCommand();
    }

    newSerialData = false;
    //reset the parsed tokens
    for (int i = 0; i < numTokens; ++i)
    {
        incomingParams[i][0] = '\0';
    }
    //reset the token counter
    numTokens = 0;
}

void startMeasurement()
{
    if (isDebugging)
    {
        Serial.println("Starting measurement");
    }
    
    current = INITIAL_CURRENT / CURRENT_FACTOR;
    double voltageMeasured = 0.0;
    
    while (voltageMeasured < VOLTAGE_MIN_MEASURE)
    {
        // choose the desired current
        current = current * CURRENT_FACTOR;
        if (isDebugging)
        {
            Serial.print("Current set to: ");
            Serial.println(current,12);
        }

        // choose the appropriate series resistor
        openAllRseries();
        double Rseries = 10000000.0;
        
        if (current < MAX_CURRENT_10M)
        {
            digitalWrite(RELAY_10M, HIGH);
            Rseries = 10000000.0;
        }
        else if (current < MAX_CURRENT_1M)
        {
            digitalWrite(RELAY_1M, HIGH);
            Rseries = 1000000.0;
        }
        else if (current < MAX_CURRENT_100k)
        {
            digitalWrite(RELAY_100k, HIGH);
            Rseries = 100000.0;
        }
        else if (current < MAX_CURRENT_10k)
        {
            digitalWrite(RELAY_10k, HIGH);
            Rseries = 10000.0;
        }
        else if (current < MAX_CURRENT_1k)
        {
            digitalWrite(RELAY_1k, HIGH);
            Rseries = 1000.0;
        }
        else
        {
            Rseries = 1000.0;
            current = current / CURRENT_FACTOR;
            printSUCC;
            Serial.println("Smallest Rseries reached");
            break;
        }
        delay(DELAY_SWITCH);

        // Set voltage to achieve current
        double Vdac = current * Rseries;
        setDAC(Vdac);

        if (isDebugging)
        {
            Serial.print("Rseries set to: ");
            Serial.println(Rseries);
            Serial.print("DAC voltage set to: ");
            Serial.println(Vdac);
        }
        delay(DELAY_SWITCH);

        // start measuring
        voltageMeasured = 0.0;
        int sampleCount = 0;
        while (sampleCount < NUM_SAMPLES)
        {
            int16_t diff = ads1115.readADC_Differential_0_1();;
            double voltageDiff = diff * VOLTS_PER_BIT;

            // Had weird behavior with abs returning int (even without functions inside abs) so using this instead
            if (voltageDiff < 0.0)
            {
              voltageDiff = -1.0 * voltageDiff;
            }
            
            voltageMeasured += voltageDiff;

            if (isDebugging)
            {
                Serial.print("Measured Voltage #");
                Serial.print(sampleCount);
                Serial.print(": ");
                Serial.println(voltageDiff, 12);
            }
            
            sampleCount += 1;
            delay(SAMPLE_INTERVAL);
        }

        // Average the measurements
        voltageMeasured = voltageMeasured / (double)NUM_SAMPLES;
        if (isDebugging)
        {
            Serial.print("Average measured Voltage: ");
            Serial.println(voltageMeasured, 12);
        }
        
    }
    // need to handle case where reach smallest Rseries, so that it doesnt execute below
    double Rsh = 4.53236 * voltageMeasured / INAMP_GAIN / current;
    printSUCC;
    Serial.print("Sheet resistance = ");
    Serial.println(Rsh);

    openAllRseries();
    setDAC(0.0);
}

void openAllRseries()
{
    digitalWrite(RELAY_1k, LOW);
    digitalWrite(RELAY_10k, LOW);
    digitalWrite(RELAY_100k, LOW);
    digitalWrite(RELAY_1M, LOW);
    digitalWrite(RELAY_10M, LOW);
}

void setDAC(double voltage)
{
    int dacValue = (int)round(voltage/DAC_VREF * 4095.0);
    Wire.beginTransmission(MCP4725_ADDR);
    Wire.write(64);
    Wire.write(dacValue >> 4);
    Wire.write((dacValue & 15) << 4);
    Wire.endTransmission();
}

void forwardDirection()
{
    digitalWrite(DIR1, LOW);
    digitalWrite(DIR2, LOW);
}

void reverseDirection()
{
    digitalWrite(DIR1, HIGH);
    digitalWrite(DIR2, HIGH);
}

void readNextSerial()
{
    //read serial buffer until newline, read into incomingChars
    static bool hasReadHeader = false;
    static int ndx = 0;
    char nextChar;
    char footer = '\n';
    char header = '>';

    while (Serial.available() > 0 && newSerialData == false)
    {
        nextChar = Serial.read();

        if (hasReadHeader == true)
        {
            //keep adding into cstring until newline
            if (nextChar != footer)
            {
                incomingChars[ndx] = nextChar;
                ++ndx;
                if (ndx >= maxChars)
                {
                    ndx = maxChars - 1;
                }
            }
            else
            {
                //if reading newline then terminate cstring and indicate there is new data to process
                incomingChars[ndx] = '\0';
                ndx = 0;
                newSerialData = true;
                hasReadHeader = false;
            }
        }
        else if (nextChar == header)
        {
            hasReadHeader = true;
        }
    }
}

void parseTokens()
{
    char* token;
    numTokens = 0;

    token = strtok(tempChars, " ");

    while (token != NULL)
    {
        //save into cstring array and read next portion
        strcpy(incomingParams[numTokens], token);
        ++numTokens;

        if (numTokens >= maxTokens)
        {
            numTokens = maxTokens - 1;
        }

        token = strtok(NULL, " ");
    }
}

void setDebugOn()
{
    if (numTokens == 1)
    {
        isDebugging = true;
        Serial.println("Debug messages turned ON");
    }
    else
    {
        if (isDebugging)  
        {
            Serial.println("Invalid number of parameters");
        }
    }
}

void setDebugOff()
{
    if (numTokens == 1)
    {
        isDebugging = false;
        //should not be able to print below if false
        //Serial.println("Debug messages turned OFF");
    }
    else
    {
        if (isDebugging)  
        {
            Serial.println("Invalid number of parameters");
        }
    }
}

void goToCommand()
{
    //Run function based on first parsed token
    //(cannot use switch/case for cstring comparisons, it is actually better with the if-else ladder in this particular situation)
    if (strcmp(incomingParams[0], "meas") == 0)
    {   
        startMeasurement();
    }
    else if (strcmp(incomingParams[0], "forw") == 0)
    {
        forwardDirection();
    }
    else if (strcmp(incomingParams[0], "rev") == 0)
    {
        reverseDirection();
    }
    else if (strcmp(incomingParams[0], "debugon") == 0)
    {
        setDebugOn();
    }
    else if (strcmp(incomingParams[0], "debugoff") == 0)
    {
        setDebugOff();
    }
    else if (strcmp(incomingParams[0], "whoru") == 0)
    {
        printWhoAmI();
    }
    else if (strcmp(incomingParams[0], "help") == 0)
    {
        printHelp();
    }
    else if (strcmp(incomingParams[0], "info") == 0)
    {
        printInfo();
    }
    else if (strcmp(incomingParams[0], "") == 0)
    {
        //Serial.println("Blank token");
        //this happens if only header and footer are sent
    }
    else
    {
        //no command exists
        printNAK();
        Serial.print("No command \"");
        Serial.print(incomingParams[0]);
        Serial.println("\" exists. Type \"help\' for more info.");
    }
}

void printWhoAmI()
{
    Serial.println(F("This is an Arduino controller for a 4pp source measure unit"));
}

void printHelp()
{
   
}

void printInfo()
{
   
}

//printing control char utility functions
void printACK()
{
    Serial.print(charACK);
    Serial.print(": ");
}

void printlnACK()
{
    Serial.print(charACK);
    Serial.print(": ");
    Serial.println(incomingChars);
}

void printNAK()
{
    Serial.print(charNAK);
    Serial.print(": ");
}

void printlnNAK()
{
    Serial.println(charNAK);
}

void printSUCC()
{
    Serial.print(charSUCC);
    Serial.print(": ");
}


void printlnSUCC()
{
    Serial.println(charSUCC);
}

void printFAIL()
{
    Serial.print(charFAIL);
    Serial.print(": ");
}

void printlnFAIL()
{
    Serial.println(charFAIL);
}

void printWARN()
{
    Serial.print(charWARN);
    Serial.print(": ");
}

void printlnWARN()
{
    Serial.println(charWARN);
}
