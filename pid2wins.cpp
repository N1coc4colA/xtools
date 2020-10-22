
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/extensions/shape.h>
#ifndef NO_I18N
#include <X11/Xlocale.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <glib.h>

#define MAXSTR 1000

Display *display;
unsigned long window;
unsigned char *prop;
int screen = 0;

void check_status(int status, unsigned long window)
{
    if (status == BadWindow) {
        printf("window id # 0x%lx does not exists!", window);
        exit(1);
    }

    if (status != Success) {
        printf("XGetWindowProperty failed!");
        exit(2);
    }
}

unsigned char* get_string_property(char* property_name)
{
    Atom actual_type, filter_atom;
    int actual_format, status;
    unsigned long nitems, bytes_after;

    filter_atom = XInternAtom(display, property_name, True);
    status = XGetWindowProperty(display, window, filter_atom, 0, MAXSTR, False, AnyPropertyType,
                                &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    check_status(status, window);
    return prop;
}

unsigned long get_long_property(char* property_name)
{
    get_string_property(property_name);
    unsigned long long_property = prop[0] + (prop[1]<<8) + (prop[2]<<16) + (prop[3]<<24);
    return long_property;
}

Window *winlist (Display *disp, unsigned long *len) {
    Atom prop = XInternAtom(disp,"_NET_CLIENT_LIST",False), type;
    int form;
    unsigned long remain;
    unsigned char *list;

    errno = 0;
    if (XGetWindowProperty(disp,XDefaultRootWindow(disp),prop,0,1024,False,XA_WINDOW,
                &type,&form,len,&remain,&list) != Success) {
        perror("winlist() -- GetWinProp");
        return nullptr;
    }

    return (Window*)list;
}

#define MAX_PROPERTY_VALUE_LEN 4096

static char *get_property (Display *disp, Window win, /*{{{*/
        Atom xa_prop_type, char *prop_name, unsigned long *size) {
    Atom xa_prop_name;
    Atom xa_ret_type;
    int ret_format;
    unsigned long ret_nitems;
    unsigned long ret_bytes_after;
    unsigned long tmp_size;
    unsigned char *ret_prop;
    char *ret;

    xa_prop_name = XInternAtom(disp, prop_name, False);

    /* MAX_PROPERTY_VALUE_LEN / 4 explanation (XGetWindowProperty manpage):
     *
     * long_length = Specifies the length in 32-bit multiples of the
     *               data to be retrieved.
     */
    if (XGetWindowProperty(disp, win, xa_prop_name, 0, MAX_PROPERTY_VALUE_LEN / 4, False,
            xa_prop_type, &xa_ret_type, &ret_format,
            &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
        return nullptr;
    }

    if (xa_ret_type != xa_prop_type) {
        XFree(ret_prop);
        return nullptr;
    }

    /* null terminate the result to make string handling easier */
    tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
    gsize sized = tmp_size+1;
    ret = (char *)g_malloc(sized);
    memcpy((void*)ret, ret_prop, tmp_size);
    ret[tmp_size] = '\0';

    if (size) {
        *size = tmp_size;
    }

    XFree(ret_prop);
    return ret;
}/*}}}*/

char *winame (Display *disp, Window win) {
    Atom prop = XInternAtom(disp,"WM_NAME",False), type;
    int form;
    unsigned long remain, len;
    unsigned char *list;

    errno = 0;
    if (XGetWindowProperty(disp,win,prop,0,1024,False,XA_STRING,
                &type,&form,&len,&remain,&list) != Success) {
        perror("winlist() -- GetWinProp");
        return nullptr;
    }

    return (char*)list;
}

int convert(char num[]) {
   int len = strlen(num);
   int base = 1;
   int temp = 0;
   for (int i=len-1; i>=0; i--) {
      if (num[i]>='0' && num[i]<='9') {
         temp += (num[i] - 48)*base;
         base = base * 16;
      }
      else if (num[i]>='A' && num[i]<='F') {
         temp += (num[i] - 55)*base;
         base = base*16;
      }
   }
   return temp;
}

int main(int argc, char** argv)
{
    std::string str = "default";
    bool valid = true;

    if (argc > 1) {
        str = argv[1];
        int i = 0;
        while (i<((int)str.length())) {
            char val = argv[1][i];
            if ((val == *"0") ||
                    (val == *"1") ||
                    (val == *"2") ||
                    (val == *"3") ||
                    (val == *"4") ||
                    (val == *"5") ||
                    (val == *"6") ||
                    (val == *"7") ||
                    (val == *"8") ||
                    (val == *"9")  ) {} else {
                i = (int)str.length();
                valid = false;
            }
            i++;
        }
    } else {
        valid = false;
    }

    char *display_name = nullptr;  // could be the value of $DISPLAY

    display = XOpenDisplay(display_name);
    if (display == nullptr) {
        fprintf (stderr, "%s:  unable to open display '%s'\n", argv[0], XDisplayName (display_name));
    }
    screen = XDefaultScreen(display);

    if (valid == false) {
        std::cout << "No arg for PID found, skipping, using top win";
        window = RootWindow(display, screen);

        window = get_long_property("_NET_ACTIVE_WINDOW");

        printf("_NET_WM_PID: %lu\n", get_long_property("_NET_WM_PID"));
        printf("WM_CLASS: %s\n", get_string_property("WM_CLASS"));
        printf("_NET_WM_NAME: %s\n", get_string_property("_NET_WM_NAME"));

        XCloseDisplay(display);
        return 0;
    } else {
        std::cout << "Valid data found, looking for the win...";

        int i;
        unsigned long len;
        display = XOpenDisplay(nullptr);
        Window *list;
        if (!display) {
            puts("no display!");
            return -1;
        }

        list = (Window*)winlist(display,&len);
        std::cout << "Total of windows: " << (int)len;

        for (i=0;i<(int)len;i++) {
            window = list[i];
            if (((int)get_long_property((char *)"_NET_WM_PID")) == (atoi(argv[1]))) {
                printf("-->%s<--\n", get_string_property((char *)"_NET_WM_NAME"));
                printf("%d", (int)window);
            }
        }

        XFree(list);

        XCloseDisplay(display);
        return 0;
    }
}
