#define DEBUG 0

#include <Application.h>
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
class UnicodeInputServerMethod : public BInputServerMethod, public BHandler
{
public:
					 UnicodeInputServerMethod();
					~UnicodeInputServerMethod();
	filter_result	 Filter(BMessage *message, BList *outList);
	void			 MessageReceived(BMessage *msg);
	status_t		 MethodActivated(bool active);
private:
	bool			mEnabled;
	bool			mInTransaction;
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


UnicodeInputServerMethod::UnicodeInputServerMethod()
	:
	BInputServerMethod("Unicode", NULL),
	BHandler(),
	mEnabled(false),
	mInTransaction(false)
{
	if (be_app) {
		be_app->Lock();
		be_app->AddHandler(this);
		be_app->Unlock();
	}
}


UnicodeInputServerMethod::~UnicodeInputServerMethod()
{
	if (be_app) {
		be_app->Lock();
		be_app->RemoveHandler(this);
		be_app->Unlock();
	}
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

	BMessage *msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_CHANGED);
	msg->AddString("be:string", mHexData);
	msg->AddBool("be:confirmed", false);
	EnqueueMessage(msg);
}


void
UnicodeInputServerMethod::StopTransaction(bool send)
{
	if (!mInTransaction)
		return;

	SERIAL_PRINT(("Stopping transaction... \n"));

	BMessage *msg;

	if (send) {
		uint32 chr;
		sscanf(mHexData, "%" B_SCNx32, &chr);
		
		if (!BUnicodeChar::IsDefined(chr)) {
			SERIAL_PRINT(("Character 0x%08X is invalid according to BUnicodeChar\n", chr));
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
	}
	
	msg = new BMessage(B_INPUT_METHOD_EVENT);
	msg->AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
	EnqueueMessage(msg);
	mInTransaction = false;
}

filter_result
UnicodeInputServerMethod::HandleKey(BMessage *msg, BList *outList)
{	
	uint32 mod = (uint32) msg->GetInt32("modifiers", -1);
	SERIAL_PRINT(("Modifiers: 0x%08X\n", mod));

	if ((mod & 0xFF) > 1) { // anything but shift key
		StopTransaction(false);
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
	if (!mEnabled)
		return B_DISPATCH_MESSAGE;

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

	SERIAL_TRACE();
	SERIAL_PRINT(("Method activated: %s\n", active ? "true" : "false"));

	if (!active) {
		StopTransaction(true);
		
		if (mInTransaction)
			StopTransaction(false); // force stop the transaction
	}

	mInTransaction = false;
	return B_OK;
}


void
UnicodeInputServerMethod::MessageReceived(BMessage *msg)
{
	BHandler::MessageReceived(msg);
}
