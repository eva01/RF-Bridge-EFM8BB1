//=========================================================
// src/RF_Bridge_2_main.c: generated by Hardware Configurator
//
// This file will be updated when saving a document.
// leave the sections inside the "$[...]" comment tags alone
// or they will be overwritten!!
//=========================================================

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <SI_EFM8BB1_Register_Enums.h>                  // SFR declarations
#include "Globals.h"
#include "InitDevice.h"
#include "uart_0.h"
#include "pca_0.h"
#include "wdt_0.h"
#include "uart.h"
#include "RF_Handling.h"
#include "RF_Protocols.h"
// $[Generated Includes]
// [Generated Includes]$

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{

}

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
int main (void)
{
	SI_SEGMENT_VARIABLE(ReadUARTData, bool, SI_SEG_DATA) = true;
	SI_SEGMENT_VARIABLE(last_desired_rf_protocol, uint8_t, SI_SEG_XDATA);
	SI_SEGMENT_VARIABLE(tr_repeats, uint8_t, SI_SEG_XDATA);
	SI_SEGMENT_VARIABLE(l, uint16_t, SI_SEG_XDATA);

	// Call hardware initialization routine
	enter_DefaultMode_from_RESET();

	// enter default state
	LED = LED_OFF;
	BUZZER = BUZZER_OFF;

	T_DATA = TDATA_OFF;

	// enable UART
	UART0_init(UART0_RX_ENABLE, UART0_WIDTH_8, UART0_MULTIPROC_DISABLE);

	// start sniffing if enabled by default
#if Sniffing_On == 1
	// set desired RF protocol PT2260
	desired_rf_protocol = PT2260_INDEX;
	rf_sniffing_mode = MODE_TIMING;
	PCA0_DoSniffing(RF_CODE_RFIN);
	last_sniffing_command = RF_CODE_RFIN;
#else
	PCA0_StopSniffing();
#endif

	// enable global interrupts
	IE_EA = 1;

	for (l = 0; l < 10000; l++)
		BUZZER = BUZZER_ON;

	BUZZER = BUZZER_OFF;

	while (1)
	{
		unsigned int rxdata;
		uint8_t len;
		uint8_t position;
		uint8_t protocol_index;

		/* reset Watch Dog Timer */
		WDT0_feed();

		/*------------------------------------------
		 * check if something got received by UART
		 ------------------------------------------*/
		// read only data from uart if idle
		if (ReadUARTData)
			rxdata = uart_getc();
		else
			rxdata = UART_NO_DATA;

		if (rxdata == UART_NO_DATA)
		{
			if (uart_state == IDLE)
				l = 0;
			else
			{
				if (++l > 10000)
					BUZZER = BUZZER_ON;

				if (l > 30000)
				{
					l = 0;
					uart_state = IDLE;
					uart_command = NONE;
					BUZZER = BUZZER_OFF;
				}
			}
		}
		else
		{
			l = 0;

			// state machine for UART
			switch(uart_state)
			{
				// check if UART_SYNC_INIT got received
				case IDLE:
					if ((rxdata & 0xFF) == RF_CODE_START)
						uart_state = SYNC_INIT;
					break;

				// sync byte got received, read command
				case SYNC_INIT:
					uart_command = rxdata & 0xFF;
					uart_state = SYNC_FINISH;

					// check if some data needs to be received
					switch(uart_command)
					{
						case RF_CODE_LEARN:
							InitTimer3_ms(1, 50);
							BUZZER = BUZZER_ON;
							// wait until timer has finished
							WaitTimer3Finished();
							BUZZER = BUZZER_OFF;

							// set desired RF protocol PT2260
							desired_rf_protocol = PT2260_INDEX;
							rf_sniffing_mode = MODE_TIMING;
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_LEARN);

							// start timeout timer
							InitTimer3_ms(1, 30000);
							break;
						case RF_CODE_RFOUT:
							PCA0_StopSniffing();
							uart_state = RECEIVING;
							tr_repeats = RF_TRANSMIT_REPEATS;
							position = 0;
							len = 9;
							break;
						case RF_DO_BEEP:
							uart_state = RECEIVING;
							position = 0;
							len = 2;
							break;
						case RF_ALTERNATIVE_FIRMWARE:
							break;
						case RF_CODE_SNIFFING_ON:
							desired_rf_protocol = UNDEFINED_INDEX;
							rf_sniffing_mode = MODE_TIMING;
							PCA0_DoSniffing(RF_CODE_SNIFFING_ON);
							last_sniffing_command = RF_CODE_SNIFFING_ON;
							break;
						case RF_CODE_SNIFFING_OFF:
							// set desired RF protocol PT2260
							desired_rf_protocol = PT2260_INDEX;
							// re-enable default RF_CODE_RFIN sniffing
							rf_sniffing_mode = MODE_TIMING;
							PCA0_DoSniffing(RF_CODE_RFIN);
							last_sniffing_command = RF_CODE_RFIN;
							break;
						case RF_CODE_RFOUT_NEW:
							tr_repeats = RF_TRANSMIT_REPEATS;
							/* no break */
						case RF_CODE_RFOUT_BUCKET:
							uart_state = RECEIVE_LEN;
							break;
						case RF_CODE_SNIFFING_ON_BUCKET:
							rf_sniffing_mode = MODE_BUCKET;
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_SNIFFING_ON_BUCKET);
							break;
						case RF_CODE_LEARN_NEW:
							InitTimer3_ms(1, 50);
							BUZZER = BUZZER_ON;
							// wait until timer has finished
							WaitTimer3Finished();
							BUZZER = BUZZER_OFF;

							// enable sniffing for all known protocols
							last_desired_rf_protocol = desired_rf_protocol;
							desired_rf_protocol = UNDEFINED_INDEX;
							rf_sniffing_mode = MODE_TIMING;
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_LEARN_NEW);

							// start timeout timer
							InitTimer3_ms(1, 30000);
							break;
						case RF_CODE_ACK:
							// re-enable default RF_CODE_RFIN sniffing
							last_sniffing_command = PCA0_DoSniffing(last_sniffing_command);
							uart_state = IDLE;
							break;

						// unknown command
						default:
							uart_command = NONE;
							uart_state = IDLE;
							break;
					}
					break;

				// Receiving UART data length
				case RECEIVE_LEN:
					position = 0;
					len = rxdata & 0xFF;
					if (len > 0)
						uart_state = RECEIVING;
					else
						uart_state = SYNC_FINISH;
					break;

				// Receiving UART data
				case RECEIVING:
					RF_DATA[position] = rxdata & 0xFF;
					position++;

					if (position == len)
					{
						uart_state = SYNC_FINISH;
					}
					else if (position >= RF_DATA_BUFFERSIZE)
					{
						len = RF_DATA_BUFFERSIZE;
						uart_state = SYNC_FINISH;
					}
					break;

				// wait and check for UART_SYNC_END
				case SYNC_FINISH:
					if ((rxdata & 0xFF) == RF_CODE_STOP)
					{
						uart_state = IDLE;
						ReadUARTData = false;

						// check if AKN should be sent
						switch(uart_command)
						{
							case RF_CODE_LEARN:
							case RF_CODE_SNIFFING_ON:
							case RF_CODE_SNIFFING_OFF:
							case RF_CODE_RFIN:
							case RF_CODE_SNIFFING_ON_BUCKET:
								// send acknowledge
								uart_put_command(RF_CODE_ACK);
							case RF_CODE_ACK:
								// enable UART again
								ReadUARTData = true;
								break;
						}
					}
					break;
			}
		}

		/*------------------------------------------
		 * check command byte
		 ------------------------------------------*/
		switch(uart_command)
		{
			// do original learning
			case RF_CODE_LEARN:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					InitTimer3_ms(1, 200);
					BUZZER = BUZZER_ON;
					// wait until timer has finished
					WaitTimer3Finished();
					BUZZER = BUZZER_OFF;

					PCA0_DoSniffing(last_sniffing_command);
					uart_put_RF_CODE_Data(RF_CODE_LEARN_OK);

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable UART again
					ReadUARTData = true;
				}
				// check for learning timeout
				else if (IsTimer3Finished())
				{
					InitTimer3_ms(1, 1000);
					BUZZER = BUZZER_ON;
					// wait until timer has finished
					WaitTimer3Finished();
					BUZZER = BUZZER_OFF;

					PCA0_DoSniffing(last_sniffing_command);
					// send not-acknowledge
					uart_put_command(RF_CODE_LEARN_KO);

					// enable UART again
					ReadUARTData = true;
				}
				break;

			// do original sniffing
			case RF_CODE_RFIN:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					uart_put_RF_CODE_Data(RF_CODE_RFIN);

					// clear RF status
					RF_DATA_STATUS = 0;
				}
				break;

			// do original transfer
			case RF_CODE_RFOUT:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						tr_repeats--;
						PCA0_StopSniffing();

						// byte 0..1:	Tsyn
						// byte 2..3:	Tlow
						// byte 4..5:	Thigh
						// byte 6..7:	24bit Data
						SendTimingProtocol(
								*(uint16_t *)&RF_DATA[2],	// sync high
								*(uint16_t *)&RF_DATA[0], 	// sync low time
								*(uint16_t *)&RF_DATA[2], 	// bit 0 high time
								*(uint16_t *)&RF_DATA[4],	// bit 0 low time
								*(uint16_t *)&RF_DATA[4], 	// bit 1 high time
								*(uint16_t *)&RF_DATA[2],	// bit 1 low time
								0,							// sync bit count
								24,							// bit count
								false,						// inverse
								6);							// actual position at the data array

						break;

					// wait until data got transfered
					case RF_FINISHED:
						if (tr_repeats != 0)
						{
							if (PROTOCOL_DATA[PT2260_INDEX].REPEAT_DELAY > 0)
							{
								InitTimer3_ms(1, PROTOCOL_DATA[PT2260_INDEX].REPEAT_DELAY);
								// wait until timer has finished
								WaitTimer3Finished();
							}

							rf_state = RF_IDLE;
						}
						else
						{
							// restart sniffing if it was active
							PCA0_DoSniffing(last_sniffing_command);

							// disable RF transmit
							T_DATA = TDATA_OFF;

							// send acknowledge
							uart_put_command(RF_CODE_ACK);

							// enable UART again
							ReadUARTData = true;
						}
						break;
				}
				break;

			// do a beep
			case RF_DO_BEEP:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				InitTimer3_ms(1, *(uint16_t *)&RF_DATA[0]);
				BUZZER = BUZZER_ON;
				// wait until timer has finished
				WaitTimer3Finished();
				BUZZER = BUZZER_OFF;

				// send acknowledge
				uart_put_command(RF_CODE_ACK);

				// enable UART again
				ReadUARTData = true;
				PCA0_DoSniffing(last_sniffing_command);
				break;

			case RF_ALTERNATIVE_FIRMWARE:
				// send firmware version
				uart_put_command(FIRMWARE_VERSION);

				uart_state = IDLE;

				// enable UART again
				ReadUARTData = true;
				PCA0_DoSniffing(last_sniffing_command);
				break;

			// do new sniffing
			case RF_CODE_SNIFFING_ON:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					uart_put_RF_Data(RF_CODE_SNIFFING_ON, RF_DATA_STATUS & 0x7F);

					// clear RF status
					RF_DATA_STATUS = 0;
				}
				break;

			// transmit data on RF
			case RF_CODE_RFOUT_NEW:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						switch (RF_DATA[0])
						{
							case UNDEFINED_INDEX:
								protocol_index = 0xFF;
								break;
							default:
								protocol_index = RF_DATA[0];
								break;
						}

						tr_repeats--;
						PCA0_StopSniffing();

						// check if unknown protocol should be used
						// byte 0:		Protocol index
						// byte 1..2:	SYNC_HIGH FACTOR
						// byte 3..4:	SYNC_LOW FACTOR
						// byte 5..6:	PULSE_TIME
						// byte 7:		BIT_0_HIGH FACTOR
						// byte 8:		BIT_0_LOW FACTOR
						// byte 9:		BIT_1_HIGH FACTOR
						// byte 10:		BIT_1_LOW FACTOR
						// byte 11:		SYNC_BIT_COUNT
						// byte 12:		BIT_COUNT
						// byte 13:		INVERSE
						// byte 14..N:	RF data to send
						if (RF_DATA[0] == UNDEFINED_INDEX)
						{
							SendTimingProtocol(
									*(uint16_t *)&RF_DATA[5] * (*(uint16_t *)&RF_DATA[1]),	// sync high
									*(uint16_t *)&RF_DATA[5] * (*(uint16_t *)&RF_DATA[3]),	// sync low time
									*(uint16_t *)&RF_DATA[5] * RF_DATA[7],					// bit 0 high time
									*(uint16_t *)&RF_DATA[5] * RF_DATA[8],					// bit 0 low time
									*(uint16_t *)&RF_DATA[5] * RF_DATA[9],					// bit 1 high time
									*(uint16_t *)&RF_DATA[5] * RF_DATA[10],					// bit 1 low time
									RF_DATA[11],											// sync bit count
									RF_DATA[12],											// bit count
									RF_DATA[13],											// inverse
									14);													// actual position at the data array
						}
						// byte 0:		Protocol identifier 0x01..0x7E
						// byte 1..N:	data to be transmitted
						else
						{
							if (protocol_index != 0xFF)
							{
								SendTimingProtocol(
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].SYNC.HIGH,	// sync high
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].SYNC.LOW,	// sync low time
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].BIT0.HIGH,	// bit 0 high time
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].BIT0.LOW,	// bit 0 low time
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].BIT1.HIGH,	// bit 1 high time
										PROTOCOL_DATA[protocol_index].PULSE_TIME * PROTOCOL_DATA[protocol_index].BIT1.LOW,	// bit 1 low time
										PROTOCOL_DATA[protocol_index].SYNC_BIT_COUNT,										// sync bit count
										PROTOCOL_DATA[protocol_index].BIT_COUNT,											// bit count
										PROTOCOL_DATA[protocol_index].INVERSE,												// inverse
										1);																					// actual position at the data array
							}
							else
							{
								uart_command = NONE;
							}
						}
						break;

					// wait until data got transfered
					case RF_FINISHED:
						if (tr_repeats != 0)
						{
							if (protocol_index != 0xFF)
							{
								if (PROTOCOL_DATA[PT2260_INDEX].REPEAT_DELAY > 0)
								{
									InitTimer3_ms(1, PROTOCOL_DATA[PT2260_INDEX].REPEAT_DELAY);
									// wait until timer has finished
									WaitTimer3Finished();
								}
							}

							rf_state = RF_IDLE;
						}
						else
						{
							// restart sniffing if it was active
							PCA0_DoSniffing(last_sniffing_command);

							// disable RF transmit
							T_DATA = TDATA_OFF;

							// send acknowledge
							uart_put_command(RF_CODE_ACK);

							// enable UART again
							ReadUARTData = true;
						}
						break;
				}
				break;

			// new RF code learning
			case RF_CODE_LEARN_NEW:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					InitTimer3_ms(1, 200);
					BUZZER = BUZZER_ON;
					// wait until timer has finished
					WaitTimer3Finished();
					BUZZER = BUZZER_OFF;

					desired_rf_protocol = last_desired_rf_protocol;
					PCA0_DoSniffing(last_sniffing_command);

					uart_put_RF_Data(RF_CODE_LEARN_OK_NEW, RF_DATA_STATUS & 0x7F);

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable UART again
					ReadUARTData = true;
				}
				// check for learning timeout
				else if (IsTimer3Finished())
				{
					InitTimer3_ms(1, 1000);
					BUZZER = BUZZER_ON;
					// wait until timer has finished
					WaitTimer3Finished();
					BUZZER = BUZZER_OFF;

					desired_rf_protocol = last_desired_rf_protocol;
					PCA0_DoSniffing(last_sniffing_command);
					// send not-acknowledge
					uart_put_command(RF_CODE_LEARN_KO_NEW);

					// enable UART again
					ReadUARTData = true;
				}
				break;

			case RF_CODE_RFOUT_BUCKET:
			{
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						PCA0_StopSniffing();

						// byte 0:				number of buckets: k
						// byte 1:				number of repeats: r
						// byte 2*(1..k):		bucket time high
						// byte 2*(1..k)+1:		bucket time low
						// byte 2*k+2..N:		RF buckets to send
						bucket_count = RF_DATA[0] * 2;

						if ((bucket_count == 0) || (len < 4))
						{
							uart_command = NONE;
							break;
						}
						else
						{
							SendRFBuckets((uint16_t *)(RF_DATA+2), RF_DATA+bucket_count+2, len-bucket_count-2, RF_DATA[1]);
						}
						break;

					// wait until data got transfered
					case RF_FINISHED:
						// restart sniffing if it was active
						PCA0_DoSniffing(last_sniffing_command);

						// disable RF transmit
						T_DATA = TDATA_OFF;

						// send acknowledge
						uart_put_command(RF_CODE_ACK);

						// enable UART again
						ReadUARTData = true;
						break;
				}
				break;
			}

			case RF_CODE_SNIFFING_ON_BUCKET:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					uart_put_RF_buckets(RF_CODE_SNIFFING_ON_BUCKET);

					// clear RF status
					RF_DATA_STATUS = 0;
				}
				break;
		}
	}
}
