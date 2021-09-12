#include <Usb.h>
USB     Usb;

// Human Interface Device
#include <hidboot.h>
HIDBoot<HID_PROTOCOL_MOUSE>    HidMouse(&Usb);

/* Pin where we received the RTS signal from pin 7 on DB9 */
#define RTS_PROBE         7

/* Flash a LED on startup/moused-up */
#define LED               13


/* MS Mouse protocol */
#define USE_MS_PROTOCOL 

/* print out debug stuff, which will definitely break mouse function */
// #define DEBUG 


// ----- Mouse Report Parser
class rpt : public MouseReportParser
{
  public:
    /* Status variables for keeping track of button/probe states */
    bool left_status = false;
    bool right_status = false;
    bool middle_status = false;
    bool rts_status = false;
    
    bool left_changed = false;
    bool right_changed = false;
    bool middle_changed = false;

    
    bool event = false;
    bool event_mb = false;

    int x_status = 0;
    int y_status = 0;

  protected:
    void OnMouseMove  (MOUSEINFO *mi);
    void OnLeftButtonUp (MOUSEINFO *mi);
    void OnLeftButtonDown (MOUSEINFO *mi);
    void OnRightButtonUp  (MOUSEINFO *mi);
    void OnRightButtonDown  (MOUSEINFO *mi);
    void OnMiddleButtonUp (MOUSEINFO *mi);
    void OnMiddleButtonDown (MOUSEINFO *mi);
};
void rpt::OnMouseMove(MOUSEINFO *mi)
{
  x_status = mi->dX;
  y_status = mi->dY;
  event = true;
};
void rpt::OnLeftButtonUp (MOUSEINFO *mi)
{
  //Serial.println("L Butt Up");
  left_status = false;
  left_changed = true;
  event = true;
};
void rpt::OnLeftButtonDown (MOUSEINFO *mi)
{
  left_status = true;
  left_changed = true;
  event = true;
};
void rpt::OnRightButtonUp  (MOUSEINFO *mi)
{
  right_status = false;
  right_changed = true;
  event = true;
};
void rpt::OnRightButtonDown  (MOUSEINFO *mi)
{
  right_status = true;
  right_changed = true;
  event = true;
};
void rpt::OnMiddleButtonUp (MOUSEINFO *mi)
{
  middle_status = false;
  middle_changed = true;
  event = true;
  event_mb = true;
};
void rpt::OnMiddleButtonDown (MOUSEINFO *mi)
{
  middle_status = true;
  middle_changed = true;
  event = true;
  event_mb = true;
};
rpt                               Prs;





/* Serial mouse packet encoding routine */
/* Encodes a 3 byte mouse packet that complies with Microsoft Mouse/Logitech protocol */
void encodePacket(int x, int y, bool lb, bool rb, unsigned char* output)
{  
  /* Cap values just in case to avoid overflow errors */
  if ( x > 127 )
  {
    x = 127;
  }
  else if ( x < -127 )
  {
    x = -127;
  }
  
  if ( y > 127 )
  {
    y = 127;
  }
  else if ( y < -127 )
  {
    y = -127;
  }
  
  char cx = x;
  char cy = y;
    
  /* Packet 0 */
  output[0] = ((cx>>6)&0x3) | /* last 2 bits of X */
    (((cy>>6)&0x3)<<2) |      /* Last 2 bits of Y */
    (rb<<4)|(lb<<5)|0x40;     /* Mouse buttons and start packet bit */

  output[1] = cx&0x3f;        /* Packet 1 ( first 6 bits of X ) */
  output[2] = cy&0x3f;        /* Packet 2 ( first 6 bits of Y ) */
}


void setup()
{
  /* Set pin for RTS probe as input */
  pinMode( RTS_PROBE, INPUT );

  /* For blinking LED */
  pinMode( LED, OUTPUT );
  digitalWrite( LED, LOW );
  
  
  Serial.begin( 1200, SERIAL_7N1 );

  // Inititialise USB card
  Usb.Init();
  HidMouse.SetReportParser(0, &Prs);

  /* Flash LED to indicate adapter is ready */
  delay( 500 );
  for ( int i=0; i<4; i++ )
  {
    digitalWrite( LED, HIGH );
    delay( 200 );
    digitalWrite( LED, LOW );
    delay( 200 );
  }
}


void loop()
{
  Prs.event = false;
  Prs.event_mb = false;

  Prs.left_changed = false;
  Prs.right_changed = false;
  Prs.middle_changed = false;
  
  int data[2];
  unsigned char packet[4];
  int p_count = 3;


  
  int ctr = 0;
  
  /* Drop some packets.  USB is WAAAAAY too fast for serial */
  do 
  {
    Usb.Task();
    ctr++;
  } while (ctr < 475);

  
  /* Divide velocity values to smoothen the movement */
  int x_status_d = Prs.x_status; // / 2;
  int y_status_d = Prs.y_status; // / 2;
  
  /* Reset X Y counters when divided result is non-zero */
  if ( x_status_d != 0 )
  {
    Prs.x_status = 0;
    Prs.event = true;
  }

  if ( y_status_d != 0 )
  {
    Prs.y_status = 0;
    Prs.event = true;
  }
  
  /* Send mouse events if there's any */
  if ( Prs.event )
  {
    #ifdef DEBUG
    
    Serial.print( "LB:" );
    Serial.print( Prs.left_status );
    Serial.print( " RB:" );
    Serial.print( Prs.right_status );
    Serial.print( " MB:" );
    Serial.print( Prs.middle_status );
    Serial.print( " X:" );
    Serial.print( y_status_d );
    Serial.print( " Y:" );
    Serial.print( y_status_d );
    Serial.println();
    #endif

    /* Encode the packet */
    encodePacket( x_status_d, y_status_d, 
      Prs.left_status, Prs.right_status, packet );

    /* Send extra byte for the middle mouse button status */
    #ifndef USE_MS_PROTOCOL
    if ( Prs.middle_status )
    {
    
      packet[3] = 0x20;
      p_count = 4;
    }
    else
    {
      /* 4th byte is sent once when middle button is lifted */
      if ( Prs.event_mb )
      {
        
        packet[3] = 0x0;
        p_count = 4;
      }
    }
    #endif

    /* Send packet */
    #ifdef USE_SERIAL1
    Serial1.write( packet, p_count );
    #else
    Serial.write( packet, p_count );
    #endif
  }

  #ifdef USE_MS_PROTOCOL
  // Send blank packet for middle mouse status
  if ( Prs.middle_changed )
  {
    packet[0] = 0x40;
    packet[1] = 0x00;
    packet[2] = 0x00;
    p_count = 3;
    
    #ifdef USE_SERIAL1
    Serial1.write( packet, p_count );
    #else
    Serial.write( packet, p_count );
    #endif
  }
  #endif

 
  /* Send init bytes when RTS has been toggled */
  if ( digitalRead( RTS_PROBE ) )
  {
    if ( !Prs.rts_status )
    {
      #ifdef DEBUG
      Serial.print("Send init byte!\n");
      #endif
      
      delay(14);
      #ifdef USE_SERIAL1
      Serial1.write( 'M' );
      #else
      Serial.write( 'M' );
      #endif
      
      delay(63);
      
      #ifndef USE_MS_PROTOCOL
      
      #ifdef USE_SERIAL1
      Serial1.write( '3' );
      #else
      Serial.write( '3' );
      #endif

     
      #endif
      
      Prs.left_status = false;
      Prs.right_status = false;
      Prs.middle_status = false;
      
      Prs.rts_status = true;
      digitalWrite( LED, HIGH );
      delay( 500 );
      digitalWrite( LED, LOW );
    }
  }
  else
  {
    Prs.rts_status = false;
  }
  
}
