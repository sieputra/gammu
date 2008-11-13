#include <gammu.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

GSM_StateMachine *s;
INI_Section *cfg;
GSM_Error error;
char buffer[100];
volatile GSM_Error sms_send_status;
volatile bool gshutdown = false;

/* Handler for SMS send reply */
void send_sms_callback (GSM_StateMachine *sm, int status, int MessageReference)
{
	dbgprintf("Sent SMS on device: \"%s\"\n", GSM_GetConfig(sm, -1)->Device);
	if (status==0) {
		printf("..OK");
		sms_send_status = ERR_NONE;
	} else {
		printf("..error %i", status);
		sms_send_status = ERR_UNKNOWN;
	}
	printf(", message reference=%d\n", MessageReference);
}

/* Function to handle errors */
void error_handler()
{
	if (error != ERR_NONE) {
		printf("ERROR: %s\n", GSM_ErrorString(error));
		if (GSM_IsConnected(s))
			GSM_TerminateConnection(s);
		exit(error);
	}
}

/* Interrupt signal handler */
void interrupt(int sign)
{
	signal(sign, SIG_IGN);
	gshutdown = true;
}

int main(int argc UNUSED, char **argv UNUSED)
{
	GSM_SMSMessage sms;
	GSM_SMSC PhoneSMSC;
	char recipient_number[] = "+1234567890";
	char message_text[] = "Sample Gammu message";
	GSM_Debug_Info *debug_info;
	int return_value = 0;

	/* Register signal handler */
	signal(SIGINT, interrupt);
	signal(SIGTERM, interrupt);

	/* Enable global debugging to stderr */
	debug_info = GSM_GetGlobalDebug();
	GSM_SetDebugFileDescriptor(stderr, true, debug_info);
	GSM_SetDebugLevel("textall", debug_info);

	/* Prepare message */
	/* Cleanup the structure */
	memset(&sms, 0, sizeof(sms));
	/* Encode message text */
	EncodeUnicode(sms.Text, message_text, strlen(message_text));
	/* Encode recipient number */
	EncodeUnicode(sms.Number, recipient_number, strlen(recipient_number));
	/* We want to submit message */
	sms.PDU = SMS_Submit;
	/* No UDH, just a plain message */
	sms.UDH.Type = UDH_NoUDH;
	/* We used default coding for text */
	sms.Coding = SMS_Coding_Default_No_Compression;
	/* Class 1 message (normal) */
	sms.Class = 1;

	/* Allocates state machine */
	s = GSM_AllocStateMachine();
	if (s == NULL)
		return 3;

	/*
	 * Enable state machine debugging to stderr
	 * Same could be achieved by just using global debug config.
	 */
	debug_info = GSM_GetDebug(s);
	GSM_SetDebugGlobal(false, debug_info);
	GSM_SetDebugFileDescriptor(stderr, true, debug_info);
	GSM_SetDebugLevel("textall", debug_info);

	/* Find configuration file */
	error = GSM_FindGammuRC(&cfg);
	error_handler();

	/* Read it */
	error = GSM_ReadConfig(cfg, GSM_GetConfig(s, 0), 0);
	error_handler();

	/* We have one valid configuration */
	GSM_SetConfigNum(s, 1);

	/* Connect to phone */
	/* 3 means number of replies you want to wait for */
	error = GSM_InitConnection(s, 3);
	error_handler();

	/* Set callback for message sending */
	/* This needs to be done after initiating connection */
	GSM_SetSendSMSStatusCallback(s, send_sms_callback);

	/* We need to know SMSC number */
	PhoneSMSC.Location = 1;
	error = GSM_GetSMSC(s, &PhoneSMSC);
	error_handler();

	/* Set SMSC number in message */
	CopyUnicodeString(sms.SMSC.Number, PhoneSMSC.Number);

	/* Send message */
	error = GSM_SendSMS(s, &sms);
	error_handler();

	/* Wait for network reply */
	sms_send_status = ERR_TIMEOUT;
	while (!gshutdown) {
		GSM_ReadDevice(s, true);
		if (sms_send_status == ERR_NONE) {
			/* Message sent OK */
			return_value = 0;
			break;
		}
		if (sms_send_status != ERR_TIMEOUT) {
			/* Message sending failed */
			return_value = 100;
			break;
		}
	}

	/* Terminate connection */
	error = GSM_TerminateConnection(s);
	error_handler();
	return return_value;
}

/* Editor configuration
 * vim: noexpandtab sw=8 ts=8 sts=8 tw=72:
 */
