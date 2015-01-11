#include <Debug.h>
#include <List.h>
#include <Message.h>

#include <add-ons/input_server/InputServerMethod.h>

extern "C" BInputServerMethod *instantiate_input_method();
class UnicodeInputServerMethod : public BInputServerMethod
{
public:
	UnicodeInputServerMethod();
	~UnicodeInputServerMethod();
	filter_result Filter(BMessage *message, BList *outList);
	status_t MethodActivated(bool active);
private:
	bool mEnabled;
	int32 mCode;
};

BInputServerMethod *instantiate_input_method()
{
	return new UnicodeInputServerMethod();
}

UnicodeInputServerMethod::UnicodeInputServerMethod()
	:
	BInputServerMethod("Unicode", NULL),
	mEnabled(false),
	mCode(0)
{
	
}
