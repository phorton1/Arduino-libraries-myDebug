//-----------------------------------------
// myDebug.cpp
//-----------------------------------------
// 2025-03-13 Modified teensy and ESP32 (untested) to
// use a circular set of output buffers, as opposed to
// semaphores, in attempt at making this code re-entrant.

#include "myDebug.h"


int debug_level = 0;
int warning_level = 0;
Stream *dbgSerial = &Serial;
Stream *extraSerial = 0;


#if defined(CORE_TEENSY) || defined(ESP32)
    #define DISPLAY_BUFFER_SIZE     255
	#define NUM_DISPLAY_BUFFERS		5
#else
    #define DISPLAY_BUFFER_SIZE     80
#endif


#if WITH_DISPLAY || WITH_WARNINGS || WITH_ERRORS
	#if defined(CORE_TEENSY) || defined(ESP32)
		volatile static int buf_head = 0;
		volatile static int buf_tail = 0;
		static char display_buffer[NUM_DISPLAY_BUFFERS][DISPLAY_BUFFER_SIZE];

		// if there is a buffer overflow, there's no good way to display it
		// so, for now, I am not going to check buf_tail, although I will try
		// to update it when the buffer is finished being used.

		static char *get_next_display_buffer()
		{
			char *retval = display_buffer[buf_head++];
			if (buf_head == NUM_DISPLAY_BUFFERS)
				buf_head = 0;
			return retval;
		}


    #else	// arduino uses two buffers
		static char display_buffer1[DISPLAY_BUFFER_SIZE];
        static char display_buffer2[DISPLAY_BUFFER_SIZE];
			// one for unpacking PROGMEM, and the other for the sprintf
    #endif
#endif


//	#if defined(CORE_TEENSY) || defined(ESP32)
//	    volatile bool in_display = 0;
//	    static void waitSem()
//	    {
//	        while (in_display) {delay(1);}
//	        in_display = 1;
//	    }
//	    static void releaseSem()
//	    {
//	        in_display = 0;
//	    }
//	#else
    #define waitSem()
    #define releaseSem()
//  #endif


// string is "\033[%d;" with following
// ansi color codes fore back
//
//     Black 	        30 	40
//     Red 	            31 	41
//     Green 	        32 	42
//     Yellow 	        33 	43   brown
//     Blue 	        34 	44
//     Magenta 	        35 	45
//     Cyan 	        36 	46
//     White 	        37 	47
//     Bright Black 	90 	100
//     Bright Red 	    91 	101
//     Bright Green 	92 	102
//     Bright Yellow 	93 	103
//     Bright Blue 	    94 	104
//     Bright Magenta 	95 	105
//     Bright Cyan 	    96 	106
//     Bright White 	97  107


#if defined(CORE_TEENSY) || defined(ESP32)
    const char *PLATFORM_COLOR_STRING = "\033[92m";      // bright green
    const char *WARNING_COLOR_STRING  = "\033[93m";       // yellow
    const char *ERROR_COLOR_STRING    = "\033[91m";       // red

	void setColorString(int what, const char *str)
    {
        if (what == COLOR_CONST_DEFAULT)
            PLATFORM_COLOR_STRING = str;
        else if (what == COLOR_CONST_WARNING)
            WARNING_COLOR_STRING = str;
        else if (what == COLOR_CONST_ERROR)
            ERROR_COLOR_STRING = str;
    }
#else
    #define PLATFORM_COLOR_STRING   "\033[96m"       // bright cyan
    #define WARNING_COLOR_STRING    "\033[93m"       // yellow
    #define ERROR_COLOR_STRING      "\033[95m"       // magenta
#endif



#if WITH_INDENTS
    int proc_level = 0;
    void indent(Stream *out_stream=0)
    {
        if (!out_stream)
            out_stream = dbgSerial;
        if (!out_stream)
            return;
        for (int i=0; i<proc_level; i++)
            out_stream->print("    ");
    }
#endif


#if WITH_DISPLAY

    void clearDisplay()
    {
        if (dbgSerial)
        {
            dbgSerial->print("\033[2J");
            dbgSerial->print("\033[3J");
        }
    }


	#if defined(CORE_TEENSY) || defined(ESP32)
		void display_string(const char *alt_color, int level, const String &str)
		{
			if (!dbgSerial && !extraSerial)
				return;
			if (level > debug_level)
				return;
			waitSem();

			#if defined(CORE_TEENSY)
				delay(2);
			#endif

			if (dbgSerial)
			{
				dbgSerial->print(alt_color?alt_color:PLATFORM_COLOR_STRING);
				#if WITH_INDENTS
					indent();
				#endif
				dbgSerial->println(str.c_str());
			}
			if (extraSerial)
			{
				extraSerial->print(alt_color?alt_color:PLATFORM_COLOR_STRING);
				#if WITH_INDENTS
					indent(extraSerial);
				#endif
				extraSerial->println(str.c_str());
			}

			releaseSem();
		}
	#endif


    void display_fxn(const char *alt_color, int level, const char *format, ...)
    {
        if (!dbgSerial && !extraSerial)
            return;
        if (level > debug_level)
            return;
        waitSem();

        #if defined(CORE_TEENSY)
            delay(2);
        #endif

		#if defined(CORE_TEENSY) || defined(ESP32)
			char *out_buffer = get_next_display_buffer();
		#else
			char *out_buffer = display_buffer2;
		#endif

        va_list var;
        va_start(var, format);

		#if defined(CORE_TEENSY) || defined(ESP32)
			vsnprintf(out_buffer,DISPLAY_BUFFER_SIZE,format,var);

        #else
            if (strlen_P(format) >= DISPLAY_BUFFER_SIZE)
            {
                dbgSerial->println(F("error - display progmem buffer overflow"));
                releaseSem();
                return;
            }
            strcpy_P(display_buffer1,format);
			vsnprintf(display_buffer2,DISPLAY_BUFFER_SIZE,display_buffer1,var);
        #endif

		if (dbgSerial)
		{
			dbgSerial->print(alt_color?alt_color:PLATFORM_COLOR_STRING);
			#if WITH_INDENTS
				indent();
			#endif
			dbgSerial->println(out_buffer);
		}
        if (extraSerial)
		{
	        extraSerial->print(alt_color?alt_color:PLATFORM_COLOR_STRING);
			#if WITH_INDENTS
				indent(extraSerial);
			#endif
            extraSerial->println(out_buffer);
		}

		#if defined(CORE_TEENSY) || defined(ESP32)
			// buf_tail *would* be used for circular buffer overflow check
			buf_tail++;
			if (buf_tail == NUM_DISPLAY_BUFFERS)
				buf_tail = 0;
		#endif

        releaseSem();
    }
#endif



#if WITH_WARNINGS
    void warning_fxn(int level, const char *format, ...)
    {
        if (!dbgSerial && !extraSerial)
            return;
        if (level > warning_level)
            return;
        waitSem();

		#if defined(CORE_TEENSY) || defined(ESP32)
			char *out_buffer = get_next_display_buffer();
		#else
			char *out_buffer = display_buffer2;
		#endif

        va_list var;
        va_start(var, format);

		#if defined(CORE_TEENSY) || defined(ESP32)
			vsnprintf(out_buffer,DISPLAY_BUFFER_SIZE,format,var);
        #else
            if (strlen_P(format) >= DISPLAY_BUFFER_SIZE)
            {
                dbgSerial->println(F("error - warning progmem buffer overflow"));
                releaseSem();
                return;
            }
            strcpy_P(display_buffer1,format);
			vsnprintf(display_buffer2,DISPLAY_BUFFER_SIZE,display_buffer1,var);
        #endif

		if (dbgSerial)
		{
			dbgSerial->print(WARNING_COLOR_STRING);
			#if WITH_INDENTS
				indent();
			#endif
			dbgSerial->print("WARNING - ");
			dbgSerial->println(out_buffer);
		}
        if (extraSerial)
        {
			extraSerial->print(WARNING_COLOR_STRING);
			#if WITH_INDENTS
				indent(extraSerial);
			#endif
            extraSerial->print("WARNING - ");
            extraSerial->println(out_buffer);
        }
        releaseSem();
    }
#endif


#if WITH_ERRORS
    void error_fxn(const char *format, ...)
    {
        if (!dbgSerial && !extraSerial)
            return;
        waitSem();

		#if defined(CORE_TEENSY) || defined(ESP32)
			char *out_buffer = get_next_display_buffer();
		#else
			char *out_buffer = display_buffer2;
		#endif

        va_list var;
        va_start(var, format);

        #if defined(CORE_TEENSY) || defined(ESP32)
        	vsnprintf(out_buffer,DISPLAY_BUFFER_SIZE,format,var);
		#else
			if (strlen_P(format) >= DISPLAY_BUFFER_SIZE)
            {
                dbgSerial->println(F("error - error progmem buffer overflow"));
                releaseSem();
                return;
            }
            strcpy_P(display_buffer1,format);
			vsnprintf(display_buffer2,DISPLAY_BUFFER_SIZE,display_buffer1,var);
        #endif

        if (dbgSerial)
		{
			dbgSerial->print(ERROR_COLOR_STRING);
			dbgSerial->print("ERROR - ");
			dbgSerial->println(out_buffer);
		}
        if (extraSerial)
        {
			extraSerial->print(ERROR_COLOR_STRING);
            extraSerial->print("ERROR - ");
            extraSerial->println(out_buffer);
        }
        releaseSem();
    }
#endif



#if WITH_DISPLAY_BYTES

    #if defined(CORE_TEENSY) || defined(ESP32)

        // optimized version of display_bytes (speed vs memory)

        #define MAX_DISPLAY_BYTES    2048
        char disp_bytes_buf[MAX_DISPLAY_BYTES];

        char *indent_buf(char *obuf)            // only called if dbgSerial != 0
        {
            if (proc_level < 0)
            {
                dbgSerial->println("WARNING: MISSING (unbalanced) proc_entry!!");
                proc_level = 0;
            }
            int i = proc_level * 4;
            while (i--) { *obuf++ = ' '; }
            return obuf;
        }

        void display_bytes(int level, const char *label, const uint8_t *buf, int len)
		{
            if (!dbgSerial) return;
            if (level > debug_level) return;
            waitSem();
            char *obuf = disp_bytes_buf;
            obuf = indent_buf(obuf);

            while (*label)
            {
                *obuf++ = *label++;
            }
            *obuf++ = ' ';

            if (!len)
            {
                strcpy(obuf," (0 bytes!!)");
                dbgSerial->println(disp_bytes_buf);
				releaseSem();
                return;
            }

            // dbgSerial->println("its not just the call to dbgSerial->println");
            // return;

            int bnum = 0;
            while (bnum < len && bnum < MAX_DISPLAY_BYTES-8)
            {
                if (bnum % 16 == 0)
                {
                    if (bnum)
                    {
                        *obuf++ = 10;   // "\r";
                        *obuf++ = 13;   // "\n";
                        obuf = indent_buf(obuf);
                        for (int i=0; i<4; i++)
                        {
                            *obuf++ = ' ';
                        }
                    }
                }

                #if 1
                    uint8_t byte = buf[bnum++];
                    uint8_t nibble = byte >> 4;
                    byte &= 0x0f;

                    *obuf++ = nibble > 9 ? ('a' + nibble-10) : ('0' + nibble);
                    *obuf++ = byte > 9 ? ('a' + byte-10) : ('0' + byte);
                    *obuf++ = ' ';

                #else
                    sprintf(obuf,"%02x ",buf[bnum++]);
                    obuf += 3;
                #endif
            }
            *obuf++ = 0;
            dbgSerial->println(disp_bytes_buf);
            releaseSem();
        }

    #else      // display_bytes() on arduino

        void display_bytes(int level, const char *label, const uint8_t *buf, int len)
        {
            if (!dbgSerial) return;
            if (level > debug_level) return;
            if (!len)
            {
                indent();
                dbgSerial->print(label);
                dbgSerial->println(" (0 bytes!!)");
                return;
            }

            char tbuf[6];
            int bnum = 0;
            while (bnum < len)
            {
                if (bnum % 16 == 0)
                {
                    if (!bnum)
                    {
                        indent();
                        dbgSerial->print(label);
                        dbgSerial->print(" ");
                    }
                    else
                    {
                        dbgSerial->println();
                        indent();
                        dbgSerial->print("    ");
                    }
                }
                sprintf(tbuf,"%02x ",buf[bnum++]);
                dbgSerial->print(tbuf);
            }
            dbgSerial->println();
        }

    #endif      // arduino version of display_bytes
#endif // WITH_DISPLAY_BYTES


#if WITH_DISPLAY_BYTES_LONG

    void display_bytes_long(int level, uint16_t addr, const uint8_t *buf, int len, Stream *use_stream)
    {
        if (!use_stream)
            use_stream = dbgSerial;
        if (!use_stream) return;
        waitSem();

        if (level > debug_level) return;
        if (!len)
        {
            indent(use_stream);
            use_stream->println("0x000000 (0 bytes!!)");
            releaseSem();
            return;
        }

        char tbuf[20];
        char char_buf[17];
        memset(char_buf,0,17);

        int bnum = 0;
        while (bnum < len)
        {
            if (bnum % 16 == 0)
            {
                if (bnum)
                {
                    use_stream->print("    ");
                    use_stream->println(char_buf);
                    memset(char_buf,0,17);
                }
                indent();
                sprintf(tbuf,"    0x%04x: ",addr + bnum);
                use_stream->print(tbuf);
            }

            uint8_t c = buf[bnum];
            sprintf(tbuf,"%02x ",c);
            use_stream->print(tbuf);
            char_buf[bnum % 16] = (c >= 32) && (c < 128) ? ((char) c) : '.';
            bnum++;
        }
        if (bnum)
        {
            while (bnum % 16 != 0) { dbgSerial->print("   "); bnum++; }
            use_stream->print("    ");
            use_stream->println(char_buf);
        }
        releaseSem();
    }

#endif  // WITH_DISPLAY_BYTES_LONG




#if !defined(CORE_TEENSY) && !defined(ESP32)
	const char *floatToStr(float f)
		// print it out with 6 decimal places
		// uses a static buffer, so only one call per display!
	{
		#if 0
			#define DIGITS	6
			#define BUFSIZE 20
		    long M = (int) f;
			f = abs(f - (double) M);
			f *= 1000000;
			long E = (long) (f + 0.5);
			char buf[BUFSIZE]; // "%d.%05d"
			snprintf(buf, BUFSIZE, "%d.%06d", M, E);
			return buf;
		#endif


		static char buf[20];
		long dec1 = (long) f;

		f -= (float) dec1;
		f *= 1000000;
		f += + 0.5;
		long dec2 = f;

		sprintf(buf,"%ld.%06ld",dec1,dec2);
		return buf;
	}


#endif

