#define DEBUG 1

#include <Application.h>
#include <Beep.h>
#include <Debug.h>
#include <Handler.h>
#include <Input.h>
#include <List.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <UnicodeChar.h>

#include <add-ons/input_server/InputServerMethod.h>

extern "C" BInputServerMethod *instantiate_input_method();
extern "C" BInputServerMethod *instantiate_input_filter();

class UnicodeInputServerMethod : public BInputServerMethod, public BHandler
{
public:
					 UnicodeInputServerMethod();
					~UnicodeInputServerMethod();
	filter_result	 Filter(BMessage *message, BList *outList);
	status_t		 MethodActivated(bool active);
private:
	bool			mEnabled;
	bool			mInTransaction;
	bool			mUnicodeShortcut;
	BString			mHexData;

	void			StartTransaction();
	void			StopTransaction(bool = false);

	void			AddCharacter(char);
	bool			CheckChar(char);

	filter_result	HandleKey(BMessage *, BList *);
};


BInputServerMethod *instantiate_input_method()
{
	SET_DEBUG_ENABLED(1);
	return new UnicodeInputServerMethod();
}


BInputServerMethod *instantiate_input_filter()
{
	SET_DEBUG_ENABLED(1);
	return new UnicodeInputServerMethod();
}


UnicodeInputServerMethod::UnicodeInputServerMethod()
	:
	BInputServerMethod("Unicode", NULL),
	BHandler(),
	mEnabled(false),
	mInTransaction(false),
	mUnicodeShortcut(false)
{
}


UnicodeInputServerMethod::~UnicodeInputServerMethod()
{
}


void
UnicodeInputServerMethod::StartTransaction()
{
	SERIAL_PRINT(("Starting transaction\n"));
	mInTransaction = true;

	BMessage *msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_STARTED);
	msg->AddMessenger("be:reply_to", BMessenger(this));
	EnqueueMessage(msg); 

	msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
	msg->AddString("be:string", "U+");
	msg->AddBool("be:confirmed", false);
	EnqueueMessage(msg);

	mHexData = "";
}


bool
UnicodeInputServerMethod::CheckChar(char c)
{
	SERIAL_PRINT(("Checking character '%c' (0x%02X)\n", c, c));

	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
		|| (c >= 'a' && c <= 'f') || c == B_BACKSPACE;
}

void
UnicodeInputServerMethod::AddCharacter(char c)
{
	if (!mInTransaction && c == B_BACKSPACE)
		return;

	if (!mInTransaction)
		StartTransaction();

	if (c == B_BACKSPACE)
		mHexData.Truncate(mHexData.Length() - 1);
	else
		mHexData += c;

	SERIAL_PRINT(("Input method data changed: '%s'\n", mHexData.String()));

	BString resultStr = "U+";
	resultStr.Append(mHexData);

	BMessage *msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
	msg->AddString("be:string", resultStr);
	msg->AddBool("be:confirmed", false);
	EnqueueMessage(msg);
}


void
UnicodeInputServerMethod::StopTransaction(bool send)
{
	if (!mInTransaction)
		return;

	SERIAL_PRINT(("Stopping transaction... \nHexData = '%s'\n", mHexData));

	BMessage *msg;

	if (send && mHexData.Length() > 0) {
		uint32 chr = 0;
		sscanf(mHexData, "%" B_SCNx32, &chr);
		
		if (!BUnicodeChar::IsDefined(chr)) {
			SERIAL_PRINT(("Character 0x%08X is invalid according to BUnicodeChar\n", chr));
			beep();
			return; // ignore invalid character
		}

		char buffer[5] = {0};
		char *buffer_ptr = buffer;
		
		BUnicodeChar::ToUTF8(chr, &buffer_ptr);
		
		msg = new BMessage(B_INPUT_METHOD_EVENT);
		msg->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
		msg->AddString("be:string", buffer);
		msg->AddBool("be:confirmed", true);
		EnqueueMessage(msg);
	} else {
		msg = new BMessage(B_INPUT_METHOD_EVENT);
		msg->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
		msg->AddString("be:string", "");
		msg->AddBool("be:confirmed", true);
		EnqueueMessage(msg);
	}
	
	msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
	EnqueueMessage(msg);
	mInTransaction = false;

	if (mUnicodeShortcut)
		mUnicodeShortcut = false;
}

filter_result
UnicodeInputServerMethod::HandleKey(BMessage *msg, BList *outList)
{
	uint32 mod = (uint32) msg->GetInt32("modifiers", -1);
	SERIAL_PRINT(("Modifiers: 0x%08X\n", mod));

	if (mod & (B_COMMAND_KEY | B_CONTROL_KEY)) { // command / control key
		return B_DISPATCH_MESSAGE;
	}

	char c;
	const char *bytes;

	if (msg->FindString("bytes", &bytes) == B_OK) {
		c = *bytes;
	} else {
		uint8 byte[4] = {0};
		ssize_t bytes = 3;

		if (msg->FindData("byte", B_UINT8_TYPE, (const void **)&byte, &bytes) != B_OK)
			return B_DISPATCH_MESSAGE;
		c = *byte;
	}

	if (c == B_ESCAPE) {
		StopTransaction(false);
	}

	if (msg->what == B_KEY_DOWN) {
		if (CheckChar(c))
			AddCharacter(c);

		return B_SKIP_MESSAGE;
	} else if (msg->what == B_KEY_UP) {
		if (mInTransaction && (c == ' ' || c == '\n'))
			StopTransaction(true);

		return B_SKIP_MESSAGE;
	}

	return B_DISPATCH_MESSAGE;
}


filter_result
UnicodeInputServerMethod::Filter(BMessage *msg, BList *outList)
{
	if (!mEnabled && !mUnicodeShortcut) {
		if (msg->what == B_KEY_DOWN) {
			uint32 mod = (uint32) msg->GetInt32("modifiers", -1);
			SERIAL_PRINT(("Unmapped modifiers: 0x%08X\n", mod));
			if ((mod & (B_COMMAND_KEY | B_SHIFT_KEY | B_CONTROL_KEY)) == (B_COMMAND_KEY | B_SHIFT_KEY)) {
				if (msg->GetInt32("raw_char", -1) == 'u') {
					mUnicodeShortcut = true;
					SERIAL_PRINT(("Got command + shift + u command!\n"));
					StartTransaction();
					return B_SKIP_MESSAGE;
				}
			}
		}
		return B_DISPATCH_MESSAGE;
	}

	switch(msg->what) {
	case B_KEY_DOWN:
	case B_KEY_UP:
		return HandleKey(msg, outList);
	default:
		return B_DISPATCH_MESSAGE;
	}
}


status_t
UnicodeInputServerMethod::MethodActivated(bool active)
{
	mEnabled = active;
	mHexData = "";

	SERIAL_PRINT(("Method activated: %s\n", active ? "true" : "false"));

	if (!active) {
		StopTransaction(true);
		
		if (mInTransaction)
			StopTransaction(false); // force stop the transaction
	}

	mInTransaction = false;
	return B_OK;
}
