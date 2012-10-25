// StifleStand V1.1 - written by http on 8/9 October 2012,
// updated/rewritten on 10 October 2012 with code update from StifleStand for Mac V1.0.3
// basic structure of this file by comex (Spirit jailbreak)
// code to hide icon by Filippo Bigarella (StifleStand for Mac V1.0.3)
// problems to fix:
// - better error handling, log files
// - make it independent of iTunes DLLs

#define _CRT_SECURE_NO_WARNINGS
#include "targetver.h"
#include <windows.h>
#include <wininet.h>
#include <direct.h>
#include "MobileDevice.h"

#define ERR_SUCCESS (mach_error_t)0
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
char *szClassName=TEXT("WindowsApp");
HWND window=NULL;
HWND button=NULL;
HWND title=NULL;
HWND status=NULL;
HWND edit=NULL;
HHOOK hMsgBoxHook;

char *firmwareVersion=NULL;
char *deviceModel=NULL;
BOOL deviceConnected=FALSE;
BOOL hideNewsstandSucceeded=FALSE;
BOOL isHidingNewsstand=FALSE;
char cfpath[MAX_PATH];
char mdpath[MAX_PATH];
DWORD exitCode=0;
char *output=NULL;
static struct am_device *device=NULL;

HMODULE mobile_device_framework;
int (*am_device_notification_subscribe)(void *, int, int, void *, am_device_notification **);
CFStringRef (*am_copy_value)(am_device *, CFStringRef, CFStringRef);
mach_error_t (*am_device_connect)(struct am_device *device);
mach_error_t (*am_device_is_paired)(struct am_device *device);
mach_error_t (*am_device_validate_pairing)(struct am_device *device);
mach_error_t (*am_device_start_session)(struct am_device *device);
mach_error_t (*am_device_start_service)(struct am_device *device, CFStringRef service_name, service_conn_t *handle, unsigned int *unknown);
mach_error_t (*am_device_release)(struct am_device *device);
mach_error_t (*am_device_stop_session)(struct am_device *device);
mach_error_t (*am_device_disconnect)(struct am_device *device);

HMODULE corefoundation;
int (*cf_string_get_cstring)(CFStringRef, char *, CFIndex, CFStringEncoding);
CFStringRef (*cf_string_create_with_cstring)(CFAllocatorRef, char *, CFStringEncoding);
void (*cf_release)(CFTypeRef);
CFPropertyListRef (*cf_propertylist_create_with_data)(CFAllocatorRef, CFDataRef, CFOptionFlags, CFPropertyListFormat *, CFErrorRef *);
CFStringRef (*cf_string_make_constant_string)(const char *cStr);
CFDataRef (*cf_data_create_with_bytes_no_copy)(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length, CFAllocatorRef bytesDeallocator);
const UInt8 *(*cf_data_get_byte_ptr)(CFDataRef theData);
CFIndex (*cf_data_get_length)(CFDataRef theData);
CFDataRef (*cf_property_list_create_data)(CFAllocatorRef allocator, CFPropertyListRef propertyList, CFPropertyListFormat format, CFOptionFlags options, CFErrorRef *error);
void (*cf_array_set_value_at_index)(CFMutableArrayRef theArray, CFIndex idx, const void *value);
void (*cf_array_append_value)(CFMutableArrayRef theArray, const void *value);
CFMutableArrayRef (*cf_array_create_mutable)(CFAllocatorRef allocator, CFIndex capacity, const CFArrayCallBacks *callBacks);
CFComparisonResult (*cf_string_compare)(CFStringRef theString1, CFStringRef theString2, CFOptionFlags compareOptions);
Boolean (*cf_dictionary_get_value_if_present)(CFDictionaryRef theDict, const void *key, const void **value);
const void *(*cf_array_get_value_at_index)(CFArrayRef theArray, CFIndex idx);
CFIndex (*cf_array_get_count)(CFArrayRef theArray);
CFMutableArrayRef (*cf_array_create_mutable_copy)(CFAllocatorRef allocator, CFIndex capacity, CFArrayRef theArray);
void (*cf_dictionary_set_value)(CFMutableDictionaryRef theDict, const void *key, const void *value);
CFMutableDictionaryRef (*cf_dictionary_create_mutable)(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks);
CFMutableDictionaryRef (*cf_dictionary_create_mutable_copy)(CFAllocatorRef allocator, CFIndex capacity, CFDictionaryRef theDict);
void (*cf_dictionary_remove_value)(CFMutableDictionaryRef theDict, const void *key);
CFAllocatorRef *kcf_allocator_null;
CFArrayCallBacks *kcf_type_array_callbacks;
CFAllocatorRef *kcf_allocator_default;
CFDictionaryKeyCallBacks *kcf_type_dictionary_key_call;
CFDictionaryValueCallBacks *kcf_type_dictionary_value_callbacks;

int load_core_foundation()
{
	HKEY hKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Apple Inc.\\Apple Application Support"), 0, KEY_QUERY_VALUE, &hKey);
	DWORD itunesdll_size=MAX_PATH;

	if(RegQueryValueEx(hKey, TEXT("InstallDir"), NULL, NULL, (LPBYTE)cfpath, &itunesdll_size) != ERROR_SUCCESS)
		return 1;

	// thanks geohot and psuskeels
	SetCurrentDirectory((LPCSTR)cfpath);

	char path[MAX_PATH];
	sprintf(path, "%s%s", cfpath, "CoreFoundation.dll");
	corefoundation=LoadLibrary(path);
	if(corefoundation==NULL)
		return 1;

	// strip trailing backslash
	cfpath[strlen(cfpath) - 1]=0;

	cf_release=(void (__cdecl *)(CFTypeRef)) GetProcAddress(corefoundation, "CFRelease");
	cf_propertylist_create_with_data=(CFPropertyListRef (__cdecl *)(CFAllocatorRef,CFDataRef,CFOptionFlags,CFPropertyListFormat *,CFErrorRef *)) GetProcAddress(corefoundation, "CFPropertyListCreateWithData");
	cf_string_get_cstring=(int (__cdecl *)(CFStringRef,char *,CFIndex,CFStringEncoding)) GetProcAddress(corefoundation, "CFStringGetCString");
	cf_string_create_with_cstring=(CFStringRef (__cdecl *)(CFAllocatorRef,char *,CFStringEncoding)) GetProcAddress(corefoundation, "CFStringCreateWithCString");
	cf_string_make_constant_string=(CFStringRef (__cdecl *)(const char *)) GetProcAddress(corefoundation, "__CFStringMakeConstantString");
	cf_data_create_with_bytes_no_copy=(CFDataRef (__cdecl *)(CFAllocatorRef,const UInt8 *,CFIndex,CFAllocatorRef)) GetProcAddress(corefoundation, "CFDataCreateWithBytesNoCopy");
	cf_data_get_byte_ptr=(const UInt8 *(__cdecl *)(CFDataRef)) GetProcAddress(corefoundation, "CFDataGetBytePtr");
	cf_data_get_length=(CFIndex (__cdecl *)(CFDataRef)) GetProcAddress(corefoundation, "CFDataGetLength");
	cf_property_list_create_data=(CFDataRef (__cdecl *)(CFAllocatorRef,CFPropertyListRef,CFPropertyListFormat,CFOptionFlags,CFErrorRef *)) GetProcAddress(corefoundation, "CFPropertyListCreateData");
	cf_array_set_value_at_index=(void (__cdecl *)(CFMutableArrayRef,CFIndex,const void *)) GetProcAddress(corefoundation, "CFArraySetValueAtIndex");
	cf_array_append_value=(void (__cdecl *)(CFMutableArrayRef,const void *)) GetProcAddress(corefoundation, "CFArrayAppendValue");
	cf_array_create_mutable=(CFMutableArrayRef (__cdecl *)(CFAllocatorRef,CFIndex,const CFArrayCallBacks *)) GetProcAddress(corefoundation, "CFArrayCreateMutable");
	cf_string_compare=(CFComparisonResult (__cdecl *)(CFStringRef,CFStringRef,CFOptionFlags)) GetProcAddress(corefoundation, "CFStringCompare");
	cf_dictionary_get_value_if_present=(Boolean (__cdecl *)(CFDictionaryRef,const void *,const void **)) GetProcAddress(corefoundation, "CFDictionaryGetValueIfPresent");
	cf_array_get_value_at_index=(const void *(__cdecl *)(CFArrayRef,CFIndex)) GetProcAddress(corefoundation, "CFArrayGetValueAtIndex");
	cf_array_get_count=(CFIndex (__cdecl *)(CFArrayRef)) GetProcAddress(corefoundation, "CFArrayGetCount");
	cf_array_create_mutable_copy=(CFMutableArrayRef (__cdecl *)(CFAllocatorRef,CFIndex,CFArrayRef)) GetProcAddress(corefoundation, "CFArrayCreateMutableCopy");
	cf_dictionary_set_value=(void (__cdecl *)(CFMutableDictionaryRef,const void *,const void *)) GetProcAddress(corefoundation, "CFDictionarySetValue");
	cf_dictionary_create_mutable=(CFMutableDictionaryRef (__cdecl *)(CFAllocatorRef,CFIndex,const CFDictionaryKeyCallBacks *,const CFDictionaryValueCallBacks *)) GetProcAddress(corefoundation, "CFDictionaryCreateMutable");
	cf_dictionary_create_mutable_copy=(CFMutableDictionaryRef (__cdecl *)(CFAllocatorRef,CFIndex,CFDictionaryRef)) GetProcAddress(corefoundation, "CFDictionaryCreateMutableCopy");
	cf_dictionary_remove_value=(void (__cdecl *)(CFMutableDictionaryRef,const void *)) GetProcAddress(corefoundation, "CFDictionaryRemoveValue");
	kcf_allocator_null=(CFAllocatorRef *) GetProcAddress(corefoundation, "kCFAllocatorNull");
	kcf_type_array_callbacks=(CFArrayCallBacks *) GetProcAddress(corefoundation, "kCFTypeArrayCallBacks");
	kcf_allocator_default=(CFAllocatorRef *) GetProcAddress(corefoundation, "kCFAllocatorDefault");
	kcf_type_dictionary_key_call=(CFDictionaryKeyCallBacks *) GetProcAddress(corefoundation, "kCFTypeDictionaryKeyCallBacks");
	kcf_type_dictionary_value_callbacks=(CFDictionaryValueCallBacks *) GetProcAddress(corefoundation, "kCFTypeDictionaryValueCallBacks");

	return (corefoundation==NULL||
		cf_release==NULL||
		cf_propertylist_create_with_data==NULL||
		cf_string_get_cstring==NULL||
		cf_string_create_with_cstring==NULL||
		cf_string_make_constant_string==NULL||
		cf_data_create_with_bytes_no_copy==NULL||
		cf_data_get_byte_ptr==NULL||
		cf_data_get_length==NULL||
		cf_property_list_create_data==NULL||
		cf_array_set_value_at_index==NULL||
		cf_array_append_value==NULL||
		cf_array_create_mutable==NULL||
		cf_string_compare==NULL||
		cf_dictionary_get_value_if_present==NULL||
		cf_array_get_value_at_index==NULL||
		cf_array_get_count==NULL||
		cf_array_create_mutable_copy==NULL||
		cf_dictionary_set_value==NULL||
		cf_dictionary_create_mutable==NULL||
		cf_dictionary_create_mutable_copy==NULL||
		kcf_allocator_null==NULL||
		kcf_type_array_callbacks==NULL||
		kcf_allocator_default==NULL||
		kcf_type_dictionary_key_call==NULL||
		kcf_type_dictionary_value_callbacks==NULL);
}

int load_mobile_device() 
{
	if(load_core_foundation())
		return 1;

	HKEY hKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Apple Inc.\\Apple Mobile Device Support\\Shared"), 0, KEY_QUERY_VALUE, &hKey);
	DWORD itunesdll_size=MAX_PATH;
	char dllpath[MAX_PATH];

	if(RegQueryValueEx(hKey, TEXT("iTunesMobileDeviceDLL"), NULL, NULL, (LPBYTE)dllpath, &itunesdll_size)!=ERROR_SUCCESS)
		return 1;

	mobile_device_framework=LoadLibrary(dllpath);
	if(mobile_device_framework==NULL)
		return 1;

	// strip off last path component and backslash
	_splitpath(dllpath, mdpath, mdpath+2, NULL, NULL);
	mdpath[strlen(mdpath)-1]=0;

	am_device_notification_subscribe=(int (__cdecl *)(void *,int,int,void *,am_device_notification **))GetProcAddress(mobile_device_framework, "AMDeviceNotificationSubscribe");
	am_device_connect=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework, "AMDeviceConnect");
	am_device_is_paired=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework, "AMDeviceIsPaired");
	am_device_validate_pairing=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework, "AMDeviceValidatePairing");
	am_device_start_session=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework, "AMDeviceStartSession");
	am_copy_value=(CFStringRef (__cdecl *)(am_device *,CFStringRef,CFStringRef))GetProcAddress(mobile_device_framework, "AMDeviceCopyValue");
	am_device_start_service=(mach_error_t (__cdecl *)(am_device *,CFStringRef,service_conn_t *,unsigned int *))GetProcAddress(mobile_device_framework,"AMDeviceStartService");
	am_device_release=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework, "AMDeviceRelease");
	am_device_stop_session=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework,"AMDeviceStopSession");
	am_device_disconnect=(mach_error_t (__cdecl *)(am_device *))GetProcAddress(mobile_device_framework,"AMDeviceDisconnect");

	return (mobile_device_framework==NULL||
		am_device_notification_subscribe==NULL||
		am_device_connect==NULL||
		am_copy_value==NULL||
		am_device_is_paired==NULL||
		am_device_validate_pairing==NULL||
		am_device_start_session==NULL||
		am_device_start_service==NULL||
		am_device_release==NULL||
		am_device_stop_session==NULL||
		am_device_disconnect==NULL);
}

bool send_xml_message(service_conn_t connection, CFDictionaryRef dict)
{
	bool result=false;
	if(!dict){
		//DLog(@"NULL dictionary passed to send_xml_message!");
		exitCode=101;
		return result;
	}
	CFPropertyListRef msgData=cf_property_list_create_data(NULL, dict, kCFPropertyListBinaryFormat_v1_0,0,NULL);
	if(!msgData){
		//DLog(@"Can't convert request to XML");
		exitCode=102;
		return false;
	}
	CFIndex msgLen=cf_data_get_length((CFDataRef)msgData); // added cast from CFPropertyListRef -> CFDataRef
	uint32_t size=htonl(msgLen);
	//DLog(@"Sending msg of length: %ld (size=%d)", msgLen, size);
	if(send((int)connection, (const char *)&size, sizeof(uint32_t), 0)==sizeof(size)){
		int bytesSent=send((int)connection, (const char *)cf_data_get_byte_ptr((CFDataRef)msgData), msgLen, 0);
		//NSLog(@"bytesSent: %ld\tmsgLen: %ld", bytesSent,msgLen);
		if(bytesSent==msgLen){
			//DLog(@"Message sent");
			result=true;
		}else{
			//DLog(@"Can't send message data");
			exitCode=103;
			result=false;
		}
	}else{
		//DLog(@"Can't send message size");
		exitCode=104;
		result=false;
	}
	cf_release(msgData);
	return result;
}

CFPropertyListRef receive_xml_reply(service_conn_t connection)
{
	CFPropertyListRef reply=NULL;
	int sock=(int)((uint32_t)connection);
	uint32_t size=0;
	int rc=recv(sock, (char *)&size, sizeof(size), 0);
	if(rc != sizeof(uint32_t)){
		//*error=CFStringCreateWithFormat(*kcf_allocator_default, NULL, cf_string_make_constant_string("Couldn't receive reply size (rc=%ld, expected %ld)"), rc, sizeof(uint32_t));
		//DLog(@"%@", *((NSString **)error));
		exitCode=201;
		return NULL;
	}
	size=ntohl(size);
	if(!size){
		// assuming that if we received the size, even though it is 0,
		// nothing is wrong
		// No words.
		return NULL;
	}
	unsigned char *buff=(unsigned char *)malloc(size);
	if(!buff){
		//*error=CFStringCreateWithFormat(*kcf_allocator_default, NULL, cf_string_make_constant_string("Failed to allocate reply buffer!!!"));
		//DLog(@"%@",*((NSString **)error));
		exitCode=202;
		return NULL;
	}
	unsigned char *p=buff;
	uint32_t left=size;
	while(left){
		uint32_t received=(uint32_t)recv(sock, (char *)p, left, 0);
		if(!received){
			//*error=CFStringCreateWithFormat(*kcf_allocator_default, NULL, cf_string_make_constant_string("Reply was truncated, expected %d more bytes"), left);
			//DLog(@"%@",*((NSString **)error));
			exitCode=203;
			free(buff);
			return NULL;
		}
		left -= received, p += received;
	}
	CFDataRef r=cf_data_create_with_bytes_no_copy(0,buff,size,*kcf_allocator_null);
	reply=cf_propertylist_create_with_data(0, r, kCFPropertyListImmutable, NULL, NULL);
	cf_release(r);
	free(buff);
	return reply;
}

CFMutableArrayRef process_iconState(CFArrayRef iconState, int *didFindNewsstand)
{
	CFMutableArrayRef processedState=cf_array_create_mutable(*kcf_allocator_default, 0, &(*kcf_type_array_callbacks));
	int foundNewsstand=0;
	CFIndex outerIndex=0;
	for (outerIndex=0; outerIndex<cf_array_get_count(iconState); outerIndex++){
		CFArrayRef innerArray=(CFArrayRef)cf_array_get_value_at_index(iconState, outerIndex);
		CFMutableArrayRef processedInnerArray=cf_array_create_mutable_copy(*kcf_allocator_default, 0, innerArray);
		CFIndex innerIndex=0;
		for (innerIndex=0; innerIndex<cf_array_get_count(innerArray); innerIndex++){
			CFMutableDictionaryRef iconDict=cf_dictionary_create_mutable_copy(*kcf_allocator_default, 0, (CFDictionaryRef)cf_array_get_value_at_index(innerArray, innerIndex));
			CFStringRef listType;
			if(cf_dictionary_get_value_if_present(iconDict, cf_string_make_constant_string("listType"), (const void **)&listType) && cf_string_compare(listType, cf_string_make_constant_string("newsstand"), 0)==kCFCompareEqualTo){
				//DLog(@"Found Newsstand: (%ld,%ld)",outerIndex,innerIndex);
				foundNewsstand=1;
				CFMutableDictionaryRef magicFolder=cf_dictionary_create_mutable(*kcf_allocator_default, 0, &(*kcf_type_dictionary_key_call), &(*kcf_type_dictionary_value_callbacks));
				cf_dictionary_set_value(magicFolder, cf_string_make_constant_string("displayName"), cf_string_make_constant_string("Magic"));
				cf_dictionary_set_value(magicFolder, cf_string_make_constant_string("listType"), cf_string_make_constant_string("folder"));
				CFMutableArrayRef iconLists=cf_array_create_mutable(*kcf_allocator_default, 0, &(*kcf_type_array_callbacks));
				CFMutableArrayRef singleList=cf_array_create_mutable(*kcf_allocator_default, 0, &(*kcf_type_array_callbacks));
				cf_array_append_value(singleList, iconDict);
				cf_array_append_value(iconLists, singleList);
				cf_dictionary_set_value(magicFolder, cf_string_make_constant_string("iconLists"), iconLists);
				cf_array_set_value_at_index(processedInnerArray, innerIndex, magicFolder);
				cf_release(iconLists);
				cf_release(singleList);
				cf_release(magicFolder);
			}else{
				cf_dictionary_remove_value(iconDict, cf_string_make_constant_string("iconModDate"));
				cf_array_append_value(processedInnerArray, iconDict);
				cf_release(iconDict);
			}
		}
		cf_array_append_value(processedState, processedInnerArray);
		cf_release(processedInnerArray);
	}
	*didFindNewsstand=foundNewsstand;
	if(!foundNewsstand){
		cf_release(processedState);
		return NULL;
	}
	return processedState;
}

void hide_newsstand(struct am_device *device)
{
	exitCode=(DWORD)-1;
	service_conn_t connection;
	if(am_device_start_service(device, cf_string_make_constant_string("com.apple.springboardservices"), &connection, NULL)==MDERR_OK){
		//DLog(@"Started service %@", (NSString *)AMSVC_SPRINGBOARD_SERVICES);
		CFMutableDictionaryRef dict=cf_dictionary_create_mutable(*kcf_allocator_default, 0, &(*kcf_type_dictionary_key_call), &(*kcf_type_dictionary_value_callbacks));
		cf_dictionary_set_value(dict, cf_string_make_constant_string("command"), cf_string_make_constant_string("getIconState"));
		cf_dictionary_set_value(dict, cf_string_make_constant_string("formatVersion"), cf_string_make_constant_string("2"));
		//DLog(@"Getting icon state from device");
		if(send_xml_message(connection, dict)){
			CFPropertyListRef reply=receive_xml_reply(connection);
			if(reply){
				//DLog(@"Looking for Newsstand");
				CFArrayRef iconStateArray=(CFArrayRef)reply;
				int foundNewsstand=0;
				CFMutableArrayRef processedState=process_iconState(iconStateArray, &foundNewsstand);
				if(foundNewsstand){
					cf_dictionary_remove_value(dict, cf_string_make_constant_string("formatVersion"));
					cf_dictionary_set_value(dict, cf_string_make_constant_string("command"), cf_string_make_constant_string("setIconState"));
					cf_dictionary_set_value(dict, cf_string_make_constant_string("iconState"), (CFPropertyListRef)processedState);
					cf_release(processedState);
					if(send_xml_message(connection, dict)){
						CFPropertyListRef _repl=receive_xml_reply(connection);
						if(_repl){
							//CFShow(_repl);
							exitCode=1;
							cf_release(_repl);
						}
						if(exitCode==(DWORD)-1){
							exitCode=0;
							//[delegateInstance updateLabel:@"Done!"];
						}
						//[delegateInstance.button setEnabled:NO];
					}else{
						//[delegateInstance updateLabel:@"Failed to set icon state!"];
						exitCode=2;
					}
				}else{
					//[delegateInstance updateLabel:@"Couldn't find Newsstand. Has it already been hidden?"];
					exitCode=3;
				}
				cf_release(reply);
			}else{
				//[delegateInstance updateLabel:@"Couldn't get icon state!"];
				exitCode=4;
			}
		}
		cf_release(dict);
	}else{
		exitCode=5;
	}
	//am_device_release(device);
	//am_device_stop_session(device);
	//am_device_disconnect(device);
	//device=NULL;
}

static char *device_copy_value(am_device *device, char *key)
{
	char *buf=(char *)malloc(0x21);

	CFStringRef keyref=cf_string_create_with_cstring(NULL, key, kCFStringEncodingASCII);
	CFStringRef value=am_copy_value(device, NULL, keyref);
	cf_release(keyref);
	cf_string_get_cstring(value, buf, 0x20, kCFStringEncodingASCII);
	cf_release(value);

	return buf;
}

static void device_notification_callback(am_device_notification_callback_info *info, void *foo)
{
	if(isHidingNewsstand)
		return;

	device=info->dev;
	deviceConnected=TRUE;

	if(info->msg!=ADNCI_MSG_CONNECTED)
		deviceConnected=FALSE; 
	if(am_device_connect(device))
		deviceConnected=FALSE; 
	if(!am_device_is_paired(device))
		deviceConnected=FALSE; 
	if(am_device_validate_pairing(device))
		deviceConnected=FALSE; 
	if(am_device_start_session(device))
		deviceConnected=FALSE; 

	if(deviceConnected){
		// newly connected, so assume we never tried
		hideNewsstandSucceeded=FALSE;

		firmwareVersion=device_copy_value(device, "ProductVersion");
		deviceModel=device_copy_value(device, "ProductType");

		char display[0x200];
		strcpy(display, deviceModel);
		if(!strcmp(deviceModel, "iPhone1,1"))
			strcpy(display, "iPhone 2G");
		if(!strcmp(deviceModel, "iPhone1,2"))
			strcpy(display, "iPhone 3G");
		if(!strcmp(deviceModel, "iPhone2,1"))
			strcpy(display, "iPhone 3GS");
		if(!strcmp(deviceModel, "iPhone3,1"))
			strcpy(display, "iPhone 4");
		if(!strcmp(deviceModel, "iPhone3,3"))
			strcpy(display, "iPhone 4");
		if(!strcmp(deviceModel, "iPhone4,1"))
			strcpy(display, "iPhone 4S");
		if(!strcmp(deviceModel, "iPhone5,1"))
			strcpy(display, "iPhone 5");
		if(!strcmp(deviceModel, "iPhone5,2"))
			strcpy(display, "iPhone 5");
		if(!strcmp(deviceModel, "iPod1,1"))
			strcpy(display, "iPod touch 1G");
		if(!strcmp(deviceModel, "iPod2,1"))
			strcpy(display, "iPod touch 2G");
		if(!strcmp(deviceModel, "iPod3,1"))
			strcpy(display, "iPod touch 3G");
		if(!strcmp(deviceModel, "iPod4,1"))
			strcpy(display, "iPod touch 4G");
		if(!strcmp(deviceModel, "iPod5,1"))
			strcpy(display, "iPod touch 5G");
		if(!strcmp(deviceModel, "iPad1,1"))
			strcpy(display, "iPad 1");
		if(!strcmp(deviceModel, "iPad2,1"))
			strcpy(display, "iPad 2");
		if(!strcmp(deviceModel, "iPad2,2"))
			strcpy(display, "iPad 2");
		if(!strcmp(deviceModel, "iPad2,3"))
			strcpy(display, "iPad 2");
		if(!strcmp(deviceModel, "iPad2,4"))
			strcpy(display, "iPad 2");
		if(!strcmp(deviceModel, "iPad3,1"))
			strcpy(display, "iPad 3");
		if(!strcmp(deviceModel, "iPad3,2"))
			strcpy(display, "iPad 3");
		if(!strcmp(deviceModel, "iPad3,3"))
			strcpy(display, "iPad 3");

		char text[0x200];
		sprintf(text, "Ready: %s (%s) connected.", display, firmwareVersion);
		EnableWindow(button, TRUE);
		SendMessage(button, WM_SETTEXT, (WPARAM) NULL, (LPARAM) TEXT("Hide Newsstand")); // added in V1.0.1
		SendMessage(status, WM_SETTEXT, (WPARAM) NULL, (LPARAM) text);
	}else{
		if(hideNewsstandSucceeded)
			return;
		SendMessage(status, WM_SETTEXT, (WPARAM) NULL, (LPARAM) TEXT("Please connect device."));
		SendMessage(button, WM_SETTEXT, (WPARAM) NULL, (LPARAM) TEXT("Hide Newsstand"));
		EnableWindow(button, FALSE);
	}
}

BOOL CALLBACK window_callback(HWND hwnd, LPARAM lParam) 
{
	char windowclass[0x20];
	GetClassName(hwnd, windowclass, 0x20);
	if(!_stricmp(windowclass, "BUTTON")){
		RECT r;
		POINT tl, br;

		GetWindowRect(hwnd, &r);
		tl.x=r.left;
		tl.y=r.top;
		br.x=r.right;
		br.y=r.bottom;

		ScreenToClient(GetParent(hwnd), &br);
		ScreenToClient(GetParent(hwnd), &tl);

		MoveWindow(hwnd, tl.x, tl.y + 265, br.x - tl.x, br.y - tl.y , TRUE);
		UpdateWindow(GetParent(hwnd));
	}
	// return was missing
	// documentation says:
	// "To continue enumeration, the callback function must return TRUE; to stop enumeration, it must return FALSE."
	// see http://msdn.microsoft.com/en-us/library/windows/desktop/ms633493(v=vs.85).aspx
	return FALSE;
}

LRESULT CALLBACK message_box_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
	if(nCode<0)
		return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);

	if(nCode==HCBT_ACTIVATE){
		HWND hwnd=(HWND)wParam;

		RECT r;
		GetWindowRect(hwnd, &r);
		MoveWindow(hwnd, r.left, r.top, r.right - r.left, 400, TRUE);
		UpdateWindow(hwnd);
		UpdateWindow(GetParent(hwnd));

		EnumChildWindows(hwnd, window_callback, 0);

		edit=CreateWindowEx(0, TEXT("EDIT"), NULL, WS_VISIBLE|WS_CHILD|ES_LEFT|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL, 20, 60, r.right-r.left-40, 240, hwnd, NULL, NULL, NULL);
		SendMessage(edit, WM_SETTEXT, 0, (LPARAM)output);
		SendMessage(edit, WM_SETFONT, (WPARAM)CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, TEXT("Tahoma")), TRUE);

		UnhookWindowsHookEx(hMsgBoxHook);
		return 0;
	}

	return CallNextHookEx(hMsgBoxHook, nCode, wParam, lParam);
}

LPCTSTR GetErrorText(){
	switch(exitCode){
		case 0:
			return TEXT("OK");
		case 1:
			return TEXT("Received an answer in second receive_xml_reply!");
		case 2:
			return TEXT("Failed to set icon state!");
		case 3:
			return TEXT("Couldn't find Newsstand. Has it already been hidden?");
		case 4:
			return TEXT("Couldn't get icon state!");
		case 5:
			return TEXT("DeviceStartService failed! Please reconnect.");
		case 101:
			return TEXT("send_xml_message: NULL dictionary passed!");
		case 102:
			return TEXT("send_xml_message: Can't convert request to XML!");
		case 103:
			return TEXT("send_xml_message: Can't send message data!");
		case 104:
			return TEXT("send_xml_message: Can't send message size!");
		case 201:
			return TEXT("receive_xml_reply: Couldn't receive reply size!");
		case 202:
			return TEXT("receive_xml_reply: Failed to allocate reply buffer!");
		case 203:
			return TEXT("receive_xml_reply: Reply was truncated!");
		case (DWORD)-1:
			return TEXT("general error");
		default:
			return TEXT("unknown error code");
	}
}

void performHideNewsstand()
{
	char statusText[0x200];

	isHidingNewsstand=TRUE;
	EnableWindow(button, FALSE);
	SendMessage(button, WM_SETTEXT, (WPARAM) NULL, (LPARAM) TEXT("Hiding Newsstand..."));

	hide_newsstand(device);

	EnableWindow(button, TRUE);
	if(exitCode==0){
		hideNewsstandSucceeded=TRUE;

		sprintf(statusText, "Hiding Newsstand succeeded!");
		SendMessage(button, WM_SETTEXT, (WPARAM) NULL, (LPARAM)TEXT("Quit"));
		SendMessage(status, WM_SETTEXT, (WPARAM) NULL, (LPARAM)statusText);
	}else{
		hideNewsstandSucceeded=FALSE;

		//sprintf(statusText, "Failed to hide Newsstand (error code: %x).", exitCode);
		sprintf(statusText, GetErrorText());
		SendMessage(status, WM_SETTEXT, (WPARAM) NULL, (LPARAM)statusText);
		SendMessage(button, WM_SETTEXT, (WPARAM) NULL, (LPARAM)TEXT("Retry"));
	}

	if(output)
		free(output);
	isHidingNewsstand=FALSE;
}

BOOL MessageLoop(BOOL blocking){
	MSG messages;

	if(!(blocking ? GetMessage(&messages, NULL, 0, 0) : PeekMessage(&messages, NULL, 0, 0, PM_REMOVE)))
		return FALSE;

	TranslateMessage(&messages);
	DispatchMessage(&messages);

	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nFunsterStil)
{
	// Load iTunes
	if(load_mobile_device()!=0){
		MessageBox(NULL, TEXT("StifleStand requires iTunes to function. Please install a correct version of iTunes then run StifleStand again."), TEXT("Error"), 64);
		return 0;
	}

	// Setup device callback
	am_device_notification *notif;
	am_device_notification_subscribe(device_notification_callback, 0, 0, NULL, &notif);

	// Register window class
	WNDCLASSEX wc;
	wc.cbSize=sizeof(WNDCLASSEX);
	wc.style=0;
	wc.lpfnWndProc=WindowProcedure;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(GetModuleHandle(NULL), TEXT("ID"));
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=(HBRUSH) (COLOR_BTNFACE + 1);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szClassName;
	wc.hIconSm=(HICON)LoadImage(GetModuleHandle(NULL), TEXT("ID"), IMAGE_ICON, 16, 16, 0);
	if(!RegisterClassEx(&wc))
		return 0;

	// Create the window
	window=CreateWindowEx(0, szClassName, TEXT("StifleStand V1.1"), WS_OVERLAPPED|WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, 320, 180, HWND_DESKTOP, NULL, hInstance, NULL);

	// Title label
	title=CreateWindowEx(0, TEXT("STATIC"), TEXT("StifleStand"), WS_VISIBLE|WS_CHILD|SS_CENTER, 0, 10, 320, 25, window, NULL, NULL, NULL);
	SendMessage(title, WM_SETFONT, (WPARAM) CreateFont(25, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, TEXT("Tahoma")), TRUE);

	// Info label
	title=CreateWindowEx(0, TEXT("STATIC"), TEXT("Windows version written by http\nbased on code from Filippo Bigarella and comex"), WS_VISIBLE|WS_CHILD|SS_CENTER, 30, 45, 260, 35, window, NULL, NULL, NULL);
	SendMessage(title, WM_SETFONT, (WPARAM) CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, TEXT("Tahoma")), TRUE);

	// Hide Newsstand button
	button=CreateWindowEx(0, TEXT("BUTTON"), TEXT("Hide Newsstand"), BS_PUSHBUTTON|WS_VISIBLE|WS_CHILD, 95, 92, 130, 25, window, NULL, NULL, NULL);
	SendMessage(button, WM_SETFONT, (WPARAM) CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, TEXT("Tahoma")), TRUE);
	EnableWindow(button, FALSE);

	// Status label
	status=CreateWindowEx(0, TEXT("STATIC"), TEXT("Please connect device."), WS_VISIBLE|WS_CHILD|SS_CENTER, 0, 125, 320, 18, window, NULL, NULL, NULL);
	SendMessage(status, WM_SETFONT, (WPARAM) CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH|FF_DONTCARE, TEXT("Tahoma")), TRUE);

	// Show the window
	ShowWindow(window, nFunsterStil);

	// Run the main runloop.
	while(MessageLoop(TRUE));

	return 0;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message){
		case WM_COMMAND:
			if(hideNewsstandSucceeded){
				PostQuitMessage(0);
				return 0;
			}
			performHideNewsstand();
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}
