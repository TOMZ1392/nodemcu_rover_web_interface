

// Load Wi-Fi library
#include <ESP8266WiFi.h>

// Replace with your network credentials
const char *ssid = "shaunhome";
const char *password = "modernwarfare";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

void definePinModes()
{
    pinMode(14, OUTPUT); //d5
    pinMode(12, OUTPUT); //d6
    pinMode(13, OUTPUT); //d7
    pinMode(0, OUTPUT);  //d3
    pinMode(2, OUTPUT);  //d4
    pinMode(4, OUTPUT);  //d2
    pinMode(5, OUTPUT);  //d1
}
void initPinStates()
{
    digitalWrite(5, LOW);  //d1 pwm
    digitalWrite(12, LOW); //d6 pwm
    digitalWrite(4, LOW);  //d2
    digitalWrite(0, LOW);  //d3
    digitalWrite(2, LOW);  //d4
    digitalWrite(14, LOW); //d5
    digitalWrite(13, LOW); //d7
}
void setup()
{
    Serial.begin(115200);
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    //Serial.println(WiFi.localIP());
    String ip= WiFi.localIP().toString();
    Serial.println(ip);

    definePinModes(); // define pin direction
    initPinStates();  // pull all Output pins low
    server.begin();
}

// PowerTrain IO defines
#define FWD_LFT 4  //2
#define FWD_RGT 0  //d3
#define REV_LFT 2  //d4
#define REV_RGT 14 //d5
#define PWMRGT 5
#define PWMLFT 12
#define PWM_STEP 50
#define PWM_LOLIM 50
#define PWM_UPLIM 970

uint16_t pwmLft = 50, pwmRgt = 50;
uint16_t pwmSliderVal = 0;

uint16_t maxSpdLeft=0;
uint16_t maxSpdRight=0;
bool isCurveLeft =false;
bool isCurveRight = false;


void setMotorPwm(uint16_t lft, uint16_t rgt) // set speed of traction motors thru PWM
{
    if (lft <= PWM_LOLIM)
        lft = 50;
    if (lft >= PWM_UPLIM)
        lft = PWM_UPLIM;
    if (rgt <= PWM_LOLIM)
        rgt = 50;
    if (rgt >= PWM_UPLIM)
        rgt = PWM_UPLIM;
    pwmLft = lft;
    pwmRgt = rgt;
    Serial.print("Lftpwm: ");
    Serial.print(pwmLft);
    Serial.print(" Rgtpwm: ");
    Serial.println(pwmRgt);
    
    analogWrite(PWMLFT, lft);
    analogWrite(PWMRGT, rgt);
}

void pullAllLow() // emergency Stop
{
    digitalWrite(REV_LFT, LOW);
    digitalWrite(REV_RGT, LOW);
    digitalWrite(FWD_LFT, LOW);
    digitalWrite(FWD_RGT, LOW);
    //delay(200); -- need a ramp instead
}

void reverse()
{
    pullAllLow();
    digitalWrite(REV_LFT, HIGH);
    digitalWrite(REV_RGT, LOW);
    digitalWrite(FWD_LFT, HIGH);
    digitalWrite(FWD_RGT, LOW);
    isCurveRight=false;
      isCurveLeft=false;
    setMotorPwm(250 , 250);
}

bool fwd_flg = false; // if forward pwm > LO_lIM then decrement it when everytime  reverse is requested until pwm ==LO_LIM, after that call reverse()

void forward()
{
    pullAllLow();
    digitalWrite(REV_LFT, LOW);
    digitalWrite(REV_RGT, HIGH);
    digitalWrite(FWD_LFT, LOW);
    digitalWrite(FWD_RGT, HIGH);
    pwmLft <= pwmRgt ? pwmLft = pwmRgt : pwmRgt = pwmLft;
    setMotorPwm(pwmLft += PWM_STEP, pwmRgt += PWM_STEP);
    fwd_flg = true;
    isCurveRight=false;
      isCurveLeft=false;
}

void cwRot()
{
    pullAllLow();
    digitalWrite(REV_LFT, LOW);
    digitalWrite(REV_RGT, HIGH);
    digitalWrite(FWD_LFT, HIGH);
    digitalWrite(FWD_RGT, LOW);
}
void ccwRot()
{
    pullAllLow();
    digitalWrite(REV_LFT, HIGH);
    digitalWrite(REV_RGT, LOW);
    digitalWrite(FWD_LFT, LOW);
    digitalWrite(FWD_RGT, HIGH);
}


void curveLeft()
{
  if(!isCurveLeft&&!isCurveRight)
  {
    maxSpdLeft=pwmRgt;
    isCurveLeft=true;
  }

  if(isCurveRight)
  {
   
    pwmRgt<maxSpdRight?pwmRgt+=PWM_STEP:maxSpdRight;
    if(pwmRgt-maxSpdRight>0)
    {
      pwmLft+=PWM_STEP;
      isCurveRight=false;
      isCurveLeft=false;
      }
   }
  short diff=pwmLft - PWM_STEP;
  if(diff>=PWM_STEP)
  {
    pwmLft -= PWM_STEP;
  }
  setMotorPwm(pwmLft , pwmRgt);
    
}

void curveRight()
{
  if(!isCurveLeft&&!isCurveRight){
   maxSpdRight=pwmLft;
   isCurveRight=true;
  }
  if(isCurveLeft){
    
    pwmLft<maxSpdLeft?pwmLft+=PWM_STEP:maxSpdLeft;
   }
    if(pwmLft-maxSpdLeft>0)
    {
      pwmRgt+=PWM_STEP;
      isCurveRight=false;
      isCurveLeft=false;
      }


  short diff=pwmRgt - PWM_STEP;
  if(diff>=PWM_STEP)
  {
    pwmRgt -= PWM_STEP;
  }
  setMotorPwm(pwmLft, pwmRgt -= PWM_STEP);
    
}
uint16_t drivePwm=0;
uint32_t left_startTi=0;
uint32_t right_startTi=0;
uint32_t speedDelta_rampStart=0;
int8_t dirHead=0;
#define TURN_STEP_DURN_MS 500
#define MAX_THROTT_LIMITER 900
#define CUTOFF_THROTT_PWM 200
#define RAMP_STEP_PWM  100
void Task_powerTrainControl()
{

  if(isFwdRequested())
  {
    setMotorPwm(drivePwm, drivePwm);
    driveFwd();
    if(dirHead>0)
    {
      if(drivePwm < MAX_THROTT_LIMITER)
      {
        drivePwm+=RAMP_STEP_PWM;
      }
      
    }
    else if(dirHead < 0)
    {
      
      
    }
    if(drivePwm>CUTOFF_THROTT_PWM)
    {
      dirHead=1;
    }
    else
    {
      dirHead=0;
    }
    
    clrFwdRequested();
  }
  
  
  if(isRightRequested())
  {    
    setMotorPwm(drivePwm-TURN_STEP_PWM , drivePwm);
    if(left_startTi-millis() > TURN_STEP_DURN_MS)
    {
      clrLeftRequest(); 
    } 
  }
  
  if(isRightRequested())
  {    
    setMotorPwm(drivePwm , drivePwm-TURN_STEP_PWM);
    if(right_startTi-millis() > TURN_STEP_DURN_MS)
    {
      clrRightRequest(); 
    } 
  }

  
  
  
}

void loop()
{
    Task_powerTrainControl();
    WiFiClient client = server.available(); // Listen for incoming clients

    if (client)
    {                                  // If a new client connects,
        Serial.println("New Client."); // print a message out in the serial port
        String currentLine = "";       // make a String to hold incoming data from the client
        while (client.connected())
        { // loop while the client's connected
            if (client.available())
            {                           // if there's bytes to read from the client,
                char c = client.read(); // read a byte, then
                //Serial.write(c);        // print it out the serial monitor
                header += c;
                if (c == '\n')
                { // if the byte is a newline character
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0)
                    {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        // turns the GPIOs on and off
                        if (header.indexOf("GET /fwd") >= 0)
                        {
                            Serial.println("FWD");
                            Serial.println("fwd pwm:");
                            Serial.println(pwmLft);

                            forward();
                        }
                        else if (header.indexOf("GET /rev") >= 0)
                        {
                            Serial.println("REVERSE");

                            setMotorPwm(PWM_LOLIM + 200, PWM_LOLIM + 200);
                            reverse();
                        }
                        else if (header.indexOf("GET /left") >= 0)
                        {
                            Serial.println("LEFT TURN");

                            curveLeft();
                        }
                        else if (header.indexOf("GET /right") >= 0)
                        {
                            Serial.println("RIGHT TURN");

                            curveRight();
                        }
                        else if (header.indexOf("GET /stop") >= 0)
                        {
                            Serial.println("Stop fired");

                            pullAllLow();
                        }
                        else if (header.indexOf("slide") >= 0)
                        {
                            Serial.println("----------Slider----------------");
                            // extract slider value from querystring
                            String val = header.substring(header.indexOf("slide") + 6, header.indexOf("slide") + 9);
                            Serial.print("Header value string: ");
                            Serial.println(val);

                            Serial.print("Header value int");
                            int sliderVal = val.substring(0, val.length()).toInt();
                            Serial.println(sliderVal);

                            pwmSliderVal = map(sliderVal, 0, 100, PWM_LOLIM - 50, PWM_UPLIM);
                            Serial.print("pwmSlider val: ");
                            Serial.println(pwmSliderVal);
                            // update speed ON THE FLY
                            if (pwmLft == pwmRgt)
                            {
                                setMotorPwm(pwmSliderVal, pwmSliderVal);
                                Serial.print("lft rgt same\n");
                            }
                            else
                            {
                                if (pwmLft > pwmRgt)
                                {
                                    float ratio = (float)(pwmRgt) / pwmLft;
                                    setMotorPwm(pwmSliderVal, pwmSliderVal * ratio);
                                    Serial.print("Right turn update: left: ");
                                    Serial.print(pwmSliderVal);
                                    Serial.print("  Right:");
                                    Serial.println(pwmSliderVal * ratio);
                                }
                                else
                                {
                                    float ratio = (float)(pwmLft) / pwmRgt;
                                    setMotorPwm(pwmSliderVal * ratio, pwmSliderVal);
                                    Serial.print("Left turn update: left: ");
                                    Serial.print(pwmSliderVal * ratio);
                                    Serial.print("  Right:");
                                    Serial.println(pwmSliderVal);
                                }
                            }
                        }

                        // ----------------- UI ---------------------------------
                        client.println("<!DOCTYPE html>");
                        client.println("<html>");
                        client.println("<head>");
                        // client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        //prevent zoom scroll
                        client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\" />");
                       
                        client.println("<style>");
                        client.println("html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");

                        client.println(".button { background-color: #f0e92b; border: none;border-radius:5px; color: white; padding: 5px 10px;");
                        client.println("text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}");
                        client.println(".button3 { position:relative;}");
                        client.println(".button2 {background-color: #3f4545;}");
                        client.println(".slidecontainer {width: 100%;}");
                        client.println(".slider {-webkit-appearance: none;width: 100%;height: 40px;background: #d3d3d3;outline: none;opacity: 0.7;-webkit-transition: .2s;transition: opacity .2s;}");
                        client.println(".slider:hover {opacity: 1;}");
                        client.println(".slider::-webkit-slider-thumb {-webkit-appearance: none;appearance: none;width: 25px;height: 40px;background: #4CAF50;cursor: pointer;}");
                        client.println("td {align=\"right\"}");
                        client.println(".slider::-moz-range-thumb {width: 25px;height: 40px;background: #4CAF50;cursor: pointer;}");
                        client.println("</style>");
                        client.println("</head>");
                        client.println("<body style=\"background-color:#b3ecff\">");
                        client.println("<h1>Node Control Pannel</h1>");
                        client.println("<table style=\" width:100%\">");
                        //client.println("<table >");
                        client.println("<tr><td></td><td><button class=\"button\" id=\"btnfwd\">FWD</button></td><td></td></tr>");
                        client.println("<tr><td><button class=\"button\" id=\"btnlft\">LEFT</button></td>");
                        client.println("<td><button class=\"button\" id=\"btnstp\" style=\"background-color:#ff0000\">STOP</button></td>");
                        client.println("<td><button class=\"button\" id=\"btnrgt\">RIGHT</button></td>");
                        client.println("<tr><td></td><td><button class=\"button\" id=\"btnrev\">REV</button></td>");
                        client.println("<td></td></tr></table>");
                        client.println("<p>Slide to adjust throttle</p>");
                        client.println("<div class=\"slidecontainer\">");
                        client.println("<input type=\"range\" min=\"0\" max=\"100\" value=\"0\" class=\"slider\" id=\"myRange\">");
                        client.println("<p>Value: <span id=\"demo\"></span></p>");
                        client.println("</div>");
                        client.println("<script> ");
                        client.println("var slider = document.getElementById(\"myRange\");");
                         client.println("var output = document.getElementById(\"demo\");");
                         
                        client.println("var btnfwd = document.getElementById(\"btnfwd\");");
                        client.println("var btnrev = document.getElementById(\"btnrev\");");
                        client.println("var btnlft = document.getElementById(\"btnlft\");");
                        client.println("var btnrgt = document.getElementById(\"btnrgt\");");
                        client.println("var btnstp = document.getElementById(\"btnstp\");");

                       
                        client.println("output.innerHTML = slider.value;");
                        client.println("slider.oninput = function() {output.innerHTML = this.value;}");
                        client.println("slider.onmouseleave = function(){");
                        client.println("console.log(\"Slider event fired \");");
                        client.println("var xhttp = new XMLHttpRequest();");
                        String ip=WiFi.localIP().toString();
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/slide/\" + this.value  , true);");
                        client.println("xhttp.send();}");
                        
                        client.println("btnfwd.onclick = function(){ ");
                        client.println("var xhttp = new XMLHttpRequest(); ");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/fwd\"  , true); ");
                        client.println("xhttp.send();} ");

                        client.println("btnrev.onclick = function(){ ");
                        client.println("var xhttp = new XMLHttpRequest(); ");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/rev\"  , true); ");
                        client.println("xhttp.send();} ");

                        client.println("btnrgt.onclick = function(){ ");
                        client.println("var xhttp = new XMLHttpRequest(); ");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/right\"  , true); ");
                        client.println("xhttp.send();} ");

                        client.println("btnlft.onclick = function(){ ");
                        client.println("var xhttp = new XMLHttpRequest(); ");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/left\"  , true); ");
                        client.println("xhttp.send();} ");

                        client.println("btnstp.onclick = function(){ ");
                        client.println("var xhttp = new XMLHttpRequest(); ");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/stop\"  , true); ");
                        client.println("xhttp.send();} ");
                        
                        client.println("slider.ontouchend = function(){");
                        client.println("console.log(\"Slider event fired \");");
                        client.println("var xhttp = new XMLHttpRequest();");
                        client.println("xhttp.open(\"GET\", \"http://"+ip+"/slide/\" + this.value  , true);");
                        client.println("xhttp.send();}</script>");
                        //client.println("}</script>");
                        client.println("</body></html>");

                        // The HTTP response ends with another blank line
                        client.println();
                        // Break out of the while loop
                        break;
                    }
                    else
                    { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                }
            }
        }

        header = "";

        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
