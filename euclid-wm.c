#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <X11/Xlib.h>
#include<X11/Xutil.h>
#include<X11/Xatom.h>
	//need this to get hints and properties
/*BASIC VARIABLE TYPES*/


/*
 * Overall structure
 * 
 * s[x] = screens
 * 		|
 * v = view
 * 		|		\
 * t = 	tracks		stack
 * 		|
 * c = 	containers
 * 		|
 * w = win
 * 
 */



struct view {
	struct view *next;
	struct view *prev;
	struct track *ft; //first track
	bool orientv; //tracks run verically?
	struct stack_item *stack;
	struct cont *mfocus; //focus
	struct stack_item *sfocus;//stackfocus
	int idx;
	bool showstack;
	bool fs; //fullscreen?
	//Window stackid; //window to display the stack
};

struct track {
	struct view *view;
	struct track *next;
	struct track *prev;
	struct cont *c;
	int size;
};

//cont = a container for a window
struct cont {
	struct track *track;
	struct cont *next;
	struct cont *prev;
	struct win *win; 
	int size; //size represents h or w depending on the orientation of the layout

};

//we keep a chain of all wins 
struct  win {
	struct win *next;
	
	bool mapped;   //mapped unmapped: note: this is the internal state
		//it does not set the state, but alows us to filter dup unmap events
	bool transient;	//transient? if it is we need to make sure we draw it 
			//over another window, use XGetTrasientForHint()

	Window id; //window id
	char *title; //use XGetWMName(dpy,w,XTextProperty *->value) to set this
			//of course it's a string so we need strcpy() to get it

	//it would make life a lot easier to keep a pointer for all the containers and stack items that 
	//point to this window
	//maintining the list might be a bitch

};

/*The STACK ITEM
 *the stack item is just a title and a win id
 *A chain of these creates the stack
 */

struct stack_item {
	struct win *win;
	struct stack_item *next;
	struct stack_item *prev;
};

struct binding {
	unsigned int keycode;
	unsigned int mask;
};


/*
 *GLOBAL VARIABLES
 */

struct view *fv = NULL;
struct view *cv = NULL; //current view
struct win *first_win = NULL;

int scrn_w = 1026;
int scrn_h = 860; 
unsigned int mod = Mod1Mask;
unsigned int mods = Mod1Mask | ShiftMask;
bool sloppy_focus = true;
struct binding bindings[50];
Display *dpy;
Window root;
unsigned long focus_pix; //border colors
unsigned long unfocus_pix;
unsigned long stack_background_pix;
unsigned long stack_focus_pix;
unsigned long stack_unfocus_pix;
bool gxerror = false;
Window stackid;
Atom wm_del_win;
Atom wm_prot;

//actually registers an individual keybinding with X
//and records the keycode in appropriate array
void bind_key(char s[12], unsigned int m, struct binding *b) {
	//char is the name of the key, e.g. 'a' 'period' 'return' 'F2' etc. 
	//m is a copy of one of the two global keymask variables: mod or mods
	//binding is the index of the global binding array
	unsigned int code;
	code = XKeysymToKeycode(dpy,XStringToKeysym(s));
	//recored the binding internally so we can detect it later:
	b->keycode = code;
	b->mask = m;
	//register the binding with X
	XGrabKey(dpy,code,m,root,True,GrabModeAsync,GrabModeAsync);

};


//handles binding the keys
//it will eventually load them from a config file
//for now its hard coded
void bind_keys() {
					
	//note that the array index is significant
	//it is the key that the event loop will use
	//to identify the event and handle it 


	//resize up down left right		4	0-3
	bind_key("y",mod,&bindings[0]);
	bind_key("u",mod,&bindings[1]);
	bind_key("i",mod,&bindings[2]);
	bind_key("o",mod,&bindings[3]);
	
	//move win to view next prev 1-0	12	4-15
	bind_key("1",mods,&bindings[4]);
	bind_key("2",mods,&bindings[5]);		
	bind_key("3",mods,&bindings[6]);
	bind_key("4",mods,&bindings[7]);
	bind_key("5",mods,&bindings[8]);
	bind_key("6",mods,&bindings[9]);
	bind_key("7",mods,&bindings[10]);
	bind_key("8",mods,&bindings[11]);
	bind_key("9",mods,&bindings[12]);
	bind_key("0",mods,&bindings[13]);
	bind_key("n",mods,&bindings[14]);
	bind_key("m",mods,&bindings[15]);

	//change view next prev 1-0		12	16-27
	bind_key("1",mod,&bindings[16]);
	bind_key("2",mod,&bindings[17]);
	bind_key("3",mod,&bindings[18]);
	bind_key("4",mod,&bindings[19]);
	bind_key("5",mod,&bindings[20]);		
	bind_key("6",mod,&bindings[21]);
	bind_key("7",mod,&bindings[22]);
	bind_key("8",mod,&bindings[23]);
	bind_key("9",mod,&bindings[24]);
	bind_key("0",mod,&bindings[25]);
	bind_key("n",mod,&bindings[26]);
	bind_key("m",mod,&bindings[27]);

	//shift window u d r l  		4 	28-31
	bind_key("h",mods,&bindings[28]);
	bind_key("j",mods,&bindings[29]);
	bind_key("k",mods,&bindings[30]);
	bind_key("l",mods,&bindings[31]);

	//show stack				1	32
	//bind_key("s",mod,&bindings[32]);
	//toggle stack
	bind_key("space",mod,&bindings[32]);

	//move to stack				1	33
	bind_key("period",mod,&bindings[33]);

	//move to main				1	34
	bind_key("comma",mod,&bindings[34]);

	//swap stack and main			1	35
	bind_key("Tab",mod,&bindings[35]);

	//swap stack up down			2	36-37
	bind_key("semicolon",mods,&bindings[36]);
	bind_key("apostrophe",mods,&bindings[37]);

	//shift main focus up down left right	4	38-41
	bind_key("h",mod,&bindings[38]);
	bind_key("j",mod,&bindings[39]);		
	bind_key("k",mod,&bindings[40]);
	bind_key("l",mod,&bindings[41]);

	//shift stack focus up down		2	42-43
	bind_key("semicolon",mod,&bindings[42]);
	bind_key("apostrophe",mod,&bindings[43]);

	//close win				2	44-45
	bind_key("Escape",mod,&bindings[44]);
	bind_key("Escape",mods,&bindings[45]);

	//run menu term				2	46-47
	bind_key("Return",mod,&bindings[46]);
	bind_key("Return",mods,&bindings[47]);

	//fullscreen				1	48
	bind_key("space",mods,&bindings[48]);

	//quit					1	49
	//bind_key("",mod,&bindings[45]);

};

/*fixes bugs in dumb programs that assume a reparenting wm
 taken from scrotwm, which took it from wmname
 */
void work_around() {
	
	Atom netwmcheck, netwmname, utf8_string;
	
	netwmcheck = XInternAtom(dpy,"_NET_SUPPORTING_WM_CHECK",False);
	netwmname = XInternAtom(dpy,"_NET_WM_NAME",False);
	utf8_string = XInternAtom(dpy,"UTF8_STRING",False);
	XChangeProperty(dpy,root,netwmcheck,XA_WINDOW,32,PropModeReplace,(unsigned char *)&root,1);
	XChangeProperty(dpy,root,netwmname,utf8_string,8,PropModeReplace,"LG3D",strlen("LG3D"));
	XSync(dpy,False);
};

/*Makes a new view*/
struct view * make_view() {
	//make the view
	struct view *ptr = (struct view *) malloc(sizeof(struct view));
	
	//it is obviously the last since we just made it
	ptr->next = NULL;
	ptr->prev = NULL;
	ptr->mfocus = NULL;
	ptr->sfocus = NULL;

	
	ptr->idx = 0;
	
	
	ptr->ft = (struct track *) malloc(sizeof(struct track)); 
	
	//define variables for track:
	ptr->ft->view = ptr;
	ptr->ft->next = NULL;
	ptr->ft->prev = NULL;
	ptr->ft->c = NULL;
	ptr->ft->size = scrn_w;
	
	ptr->orientv = true;
	ptr->stack = NULL;
	ptr->showstack = false;
	ptr->fs = false;

	return ptr;
	
 };

void remove_cont(struct cont *c) {
	
	//reset focus if necessary
				
	if (c->next != NULL) {
		c->track->view->mfocus = c->next;
	} else if (c->prev != NULL) {
		c->track->view->mfocus = c->prev;
	} else if (c->track->next != NULL) {
		c->track->view->mfocus = c->track->next->c;
	} else if (c->track->prev != NULL) {
		c->track->view->mfocus = c->track->prev->c;
	} else {
		c->track->view->mfocus = NULL;
	};

					
	//remove c and clean up
					
	//is c first in the track?
	if (c->track->c == c) {
							
		//is it only in track?
		if (c->next == NULL) {
			//is track first?
			
			if (c->track == c->track->view->ft) {
				//is track only?
				if (c->track->next == NULL) {
					//TODO, c is the only container in the view
					//what do we do? leave the track?
					
					c->track->c = NULL;
					free(c);
					//set focus to root so we still get keys!
					//it too me _*HOURS*_ to remember this
					XSetInputFocus(dpy,root,None,CurrentTime);
				} else {
					//its the first track, but there is a next
					c->track->view->ft = c->track->next;
					c->track->next->prev = NULL;
					free (c->track);
					free (c);
				};	
			} else { //not first track 
				//delete it 
				c->track->prev->next = c->track->next;
				if (c->track->next != NULL) {
					c->track->next->prev = c->track->prev;
				};
				free (c->track);
				free(c);
			}; 
						
		} else {//its just first
			c->track->c = c->next;
			c->next->prev = NULL;
			free(c);				
		};
	} else { //there a prev 
		c->prev->next = c->next;
		if (c->next != NULL) {
			c->next->prev = c->prev;
		};
		free(c);
	};
	
};


struct win * add_win(Window  id) {
	XSelectInput(dpy,id,EnterWindowMask);
	if (sloppy_focus == true) {
		XSelectInput(dpy,id,PointerMotionMask | PointerMotionHintMask);
	};
	XSetWindowBorderWidth(dpy,id,1);
//	Window par;
//	XSizeHints *size = XAllocSizeHints();
//	char **name;
//	int i;
//	XTextProperty tp;
	//XGetTransientForHint(dpy,id,&par);
	//XGetWMName(dpy,id,&tp);
	//XGetWMSizeHints(dpy,id,size,NULL,atom);
//	long user_mask;
	//XGetWMNormalHints(dpy,id,size,&user_mask);
	//XmbTextPropertyToTextList(dpy,&tp,&name,&i);
	//XGetWMSizeHints(dpy,id, . . .);
	//printf("transient for %lu\nname %s\n",par,name[0]);
	
//	if ((size->flags & PPosition) != 0) {
		//printf("supplied position: %d %d\n",size->x,size->y);
//	};
//	if ((size->flags & PSize) != 0) {
		//printf("supplied size: %dw %dh\n",size->width,size->height);
		
//	};
//	XFree(size);
	
	//let's also see which protocols it supports, so we can easily implement them later:
	// atom = a 32 bit string period. 
	//XGetWMProtocols(display, w, protocols_return, count_return)
	//Atom *a;
	//int tot;
	//XGetWMProtocols(dpy,id,(Atom **)&a,&tot);
	//just test reading them:
	//int j;
	//int k;
	
	/*
	printf("number of atoms %d\n",tot);
	for (j = 0; j < tot; j++) {
		if (*a[j] == wm_del_win) {printf("delwin set\n");};
		
	};
	*/ 
	//filter: 
	/*
	for (j=0; j<tot; j++) {
		if (a[j] == WM_TAKE_FOCUS) {
			printf("wmtakefocus true\n");
		} else if (a[j] == WM_DELETE_WINDOW) {
			printf("wmdeletewindow true\n");
	};
	*/

	struct win *p = (struct win *) malloc(sizeof(struct win));
	p->next = first_win;
	first_win = p;
	p->id = id;
	return p;
};	

void forget_win (Window id) {
	//first see whether we have a record of it
	
	if (first_win == NULL) {return;};
	
	struct win * w = first_win;
	struct win * w2 = first_win;
	
	if (w->id != id) {
		while (w->next != NULL && w->next->id != id) {
	
			w = w->next;
		};
		w2 = w; //this should be the win struct before the one we are deleting
		w = w->next;
	}; 
	
	if (w == NULL) { return;};
	
	//we have the win struct stored in w;

	struct view *v = fv;
	struct track *t;
	struct cont *c;
	
	while (v != NULL) {
		t = v->ft;
		
		while (t != NULL) {
			c = t->c;
			while (c != NULL) {
				if (c->win == w) {
					//reset focus if necessary
					if (v->mfocus == c) {
						if (c->next != NULL) {
							v->mfocus = c->next;
						} else if (c->prev != NULL) {
							v->mfocus = c->prev;
						} else if (c->track->next != NULL) {
							v->mfocus = c->track->next->c;
						} else if (c->track->prev != NULL) {
							v->mfocus = c->track->prev->c;
						} else {
							v->mfocus = NULL;
						};
					};
					
					//remove c and clean up
					
					//is c first in the track?
					if (c->track->c == c) {
							
						//is it only in track?
						if (c->next == NULL) {//remove track
							//is track first?
							if (c->track == c->track->view->ft) {
								//is track only?
								if (c->track->next == NULL) {
									//TODO, c is the only container in the view
									//what do we do?
									
									c->track->c = NULL;
									free(c);
								} else {
									//its the first track, but there is a next
									cv->ft = c->track->next;
									c->track->next->prev = NULL;
									free (c->track);
									free (c);
								};	
							} else { //not first track 
								//delete it 
								c->track->prev->next = c->track->next;
								if (c->track->next != NULL) {
									c->track->next->prev = c->track->prev;
								};
								free (c->track);
								free(c);
							}; 
							
							
						
						} else {//its just first
							c->track->c = c->next;
							c->next->prev = NULL;
							free(c);				
						};
						
					} else { //there a prev 
						c->prev->next = c->next;
						if (c->next != NULL) {
							c->next->prev = c->prev;
						};
						free(c);
					};
					
					
					
				};
				c = c->next;
			};
			t = t->next;
		};
		v = v->next;
	};
	//remove w
	//is it first?
	//is it last?
	if (first_win == w) {
		first_win = w->next;
	};
	w2->next = w->next;
	
	
	free(w);
	
};

void add_client_to_view (struct win *p, struct view *v) {
	//make a cont for it
	struct cont *c = (struct cont *) malloc  (sizeof(struct cont));
	//no we need to test all these
	if (v->mfocus != NULL) {
		c->next = v->mfocus->next;
		c->prev = v->mfocus;
		if (v->mfocus->next != NULL) {
			v->mfocus->next->prev = c;
		};
		v->mfocus->next = c;
		c->track = v->mfocus->track;
		c->size = v->mfocus->size;
	} else {
		//first client in view
		
		c->next = NULL;
		c->prev = NULL;
		c->track = v->ft;
		v->ft->c = c;
		if (v->orientv == true) {
			c->size = scrn_h;
		} else {
			c->size = scrn_w;
		};
	};
	c->win = p;
	v->mfocus = c;
};

void move_to_stack(struct cont *c) {
	//insert in stack
	
	
	
	struct stack_item *s = (struct stack_item *) malloc (sizeof(struct stack_item));
	struct stack_item *p = cv->stack;
	s->win = c->win;
	s->next = p;
	s->prev = NULL;
	
	cv->stack = s;
	cv->sfocus = s;
	if (p != NULL) {
		p->prev = s;
	};
	
	remove_cont(c);
	
};

void move_to_main() {
	//just add whatever has stack focus to the layout
	if (cv->sfocus == NULL) {return;};
	add_client_to_view(cv->sfocus->win, cv);
	
	//remove it from the stack:
	
	struct stack_item *p = cv->sfocus;
	
	//reset sfocus
	cv->sfocus = NULL;
	//what if p is NULL?
	if (p->prev != NULL) {
		cv->sfocus = p->prev;
		p->prev->next = p->next;
	};
	
	if (p->next != NULL) {
		p->next->prev = p->prev;
		if (cv->sfocus == NULL) {
			cv->sfocus = p->next;
			cv->stack = p->next;
		};

		
	};
	
	if (cv->sfocus == NULL) {
		cv->stack = NULL;
	};
	
	free(p);
};

void shift_stack_focus (bool dir) {
	//true up, false, down
	if (dir && cv->sfocus->prev != NULL) {
		cv->sfocus = cv->sfocus->prev;
	} else if (!dir && cv->sfocus->next != NULL) {
		cv->sfocus = cv->sfocus->next;
	};
	
};

bool is_top_level(Window id) {


	Window r; //root return;
	Window p; //parent return;
	Window *c; //children;
	unsigned int nc; //number of children 
	//XQueryTree(dpy,id,&r,&p,&c,&nc);
	//if (p == root) {return(true);} else {return(false);};
	gxerror = false;
	XQueryTree(dpy,root,&r,&p,&c,&nc);
	int i;
	for (i = 0; i < nc; i++) {
		if (c[i] == id) {
	   		
			XWindowAttributes wa;
			XGetWindowAttributes(dpy,id,&wa);
			if (gxerror == true) {return(false);};
			if (wa.override_redirect == true) {
				return(false);
			} else {
				return(true);
			};
	
		};
	};
	return(false);

};

short int convert_to_internal_dir(short int dir) {/*four possibilities: 
	 * 	1) we are moving down in the track
	 *  2) we are movign up in the track
	 *  3) we are moving down accross tracks
	 *  4) we are moving up accross tracks
	 */
	if (cv->orientv == true) {
		switch (dir) {
		case 1: //up
			dir = 2;
			break;
		case 2: //right
			dir = 3;
			break;
		case 3: //down
			dir = 1;
			break;
		case 4:	//left
			dir = 4;
			break;
		};
	} else {
		switch (dir) {
		case 1: 
			dir = 4;
			break;
		case 2:
			dir = 2;
			break;
		case 3:
			dir = 3;
			break;
		case 4:	
			dir = 1;
			break;
		};
	
	};
	return dir;
};
void shift_window(short int dir) {
	dir = convert_to_internal_dir(dir);
	if (dir == 1 && cv->mfocus->next != NULL) { //down in the track;
		//get a direct reference to all four nodes
		
		//make sure that we are also updating track->c if necessary 
		
		struct cont *tmpa;
		struct cont *tmpb;
		struct cont *tmpc;
		struct cont *tmpd;
		tmpa = cv->mfocus->prev;
		tmpb = cv->mfocus;
		tmpc = cv->mfocus->next;
		if (cv->mfocus->next != NULL) {
			tmpd = cv->mfocus->next->next;
		} else {tmpd = NULL;};
		if (tmpa != NULL) {
			tmpa->next = tmpc;
		} else { //a is null, b is track->c
			cv->mfocus->track->c = tmpc;
		};
		
		
		if (tmpd != NULL) {
			tmpd->prev = tmpb;
		};
		if (tmpc != NULL) {
			tmpc->prev = tmpa;
			tmpc->next = tmpb;
		};
		tmpb->prev = tmpc;
		tmpb->next = tmpd;
		
	} else if (dir == 2 && cv->mfocus->prev != NULL) {
		struct cont *tmpa;
		struct cont *tmpb;
		struct cont *tmpc;
		struct cont *tmpd;
		
		if (cv->mfocus->prev != NULL) {
			tmpa = cv->mfocus->prev->prev;
		} else {
			tmpa = NULL;
		};
		tmpb = cv->mfocus->prev;
		tmpc = cv->mfocus;
		tmpd = cv->mfocus->next;
		
		if (tmpa != NULL) {
			tmpa->next = tmpc;
		};
		if (cv->mfocus->track->c == tmpb) {
			cv->mfocus->track->c = tmpc;
		};
		
		if (tmpb != NULL) {
			
			tmpb->prev = tmpc;
			tmpb->next = tmpd;	
		};

		if (tmpd != NULL) {
			tmpd->prev = tmpb;
		};
		tmpc->prev = tmpa;
		tmpc->next = tmpb;
		
	} else if (dir == 3) {
		//check for track->next
		if (cv->mfocus->track->next != NULL) {
			
			//find position in it
			//find the midpoint of the current cont
			struct cont *p;
			p = cv->mfocus->prev;
			int s = 0;
			int t = 0;
			
			while (p != NULL) {
				s += p->size;
				p = p->prev;
				
			};
			
			s += (cv->mfocus->size / 2);
			p = cv->mfocus->track->next->c;
			
			while (p->next != NULL) {
				t += p->size;
				if (t >= s) {break;};
				p = p->next;
			};
			
			if (p != NULL) {//it never should
		
				if (cv->mfocus->prev != NULL) {
			
					cv->mfocus->prev->next = cv->mfocus->next;
				} else { //it's first 
	
					if (cv->mfocus->next != NULL) {
			
						cv->mfocus->track->c = cv->mfocus->next;
					} else {
			
						//only one in the track
						//remove the old track
						if (cv->ft == cv->mfocus->track) {
		
							cv->ft = p->track;
						} else {
						
							cv->mfocus->track->prev->next = p->track;}; 
		
						p->track->prev = cv->mfocus->track->prev;
						free(cv->mfocus->track);
					};
				};
				//wheew, now we can proceed:
				//and here
				if (cv->mfocus->next != NULL) {
					cv->mfocus->next->prev = cv->mfocus->prev;
				};
			
				//put it behind p
				struct cont *b = p->next;
				if (b != NULL) {
					b->prev = cv->mfocus;
				};
				p->next = cv->mfocus;
				cv->mfocus->prev = p;
				cv->mfocus->next = b;
				cv->mfocus->track = p->track;
			
			};
			
						
		}else{ //make a track for it
			if (cv->mfocus->track->c == cv->mfocus) { 
				if (cv->mfocus->next != NULL) {
						cv->mfocus->track->c = cv->mfocus->next;
					} else if (cv->mfocus->prev != NULL) {
						cv->mfocus->track->c = cv->mfocus->prev;
					} else {return; //premature return, because movign the window that direction doesn't make any sense
					};
				};
			struct track *ptr = (struct track *) malloc (sizeof(struct track));
			cv->mfocus->track->next = ptr;
			

			//update all the other links
			ptr->view = cv;
			ptr->next = NULL;
			ptr->prev = cv->mfocus->track;
			ptr->c = cv->mfocus;
			ptr->size = cv->mfocus->track->size;
			cv->mfocus->track = ptr;
			
			
			//patch up the hole in mfocus->track
			if (cv->mfocus->prev != NULL) 
				cv->mfocus->prev->next = cv->mfocus->next;
			if (cv->mfocus->next != NULL)
				cv->mfocus->next->prev = cv->mfocus->prev;
			cv->mfocus->next = NULL;
			cv->mfocus->prev = NULL;
			
			
			
		};

	} else if (dir == 4) { //4
		if (cv->mfocus->track->prev != NULL) {
	
			//find position in it
			//find the midpoint of the current cont
			struct cont *p;
			p = cv->mfocus->prev;
			int s = 0;
			int t = 0;
		
			while (p != NULL) {
				s += p->size;
				p = p->prev;
	
			};
			
			s += (cv->mfocus->size / 2);
	
			p = cv->mfocus->track->prev->c;
			
			while (p->next != NULL) {
				t += p->size;
				if (t >= s) {break;};
				p = p->next;
			};
			
			if (p != NULL) {//it never should
			//we are getting circular prevs, not sure how this is happening
		
				if (cv->mfocus->prev != NULL) {
					
					cv->mfocus->prev->next = cv->mfocus->next;
				} else { //it's first in the track
				
					if (cv->mfocus->next != NULL) {
				
						cv->mfocus->track->c = cv->mfocus->next;
					} else {
				
						//only one in the track
						//remove the old track
											
						if (cv->mfocus->track->next != NULL) {
							cv->mfocus->track->next->prev = p->track;
						}; 
				
						p->track->next = cv->mfocus->track->next;
						free(cv->mfocus->track);
					};
				};
				//wheew, now we can proceed:
				if (cv->mfocus->next != NULL) {
					//is this causing all the harm?
					cv->mfocus->next->prev = cv->mfocus->prev;
				};
			
				//put it behind p
				struct cont *b = p->next;
				if (b != NULL) {
					b->prev = cv->mfocus;
				};
				p->next = cv->mfocus;
				cv->mfocus->prev = p;
				cv->mfocus->next = b;
				cv->mfocus->track = p->track;
			
			};	
		}else{ //make a track for it
			if (cv->mfocus->track->c == cv->mfocus) { 
				if (cv->mfocus->next != NULL) {
						cv->mfocus->track->c = cv->mfocus->next;
					} else if (cv->mfocus->prev != NULL) {
						cv->mfocus->track->c = cv->mfocus->prev;
					} else {return; //premature return, because movign the window that direction doesn't make any sense
					};
				};
			struct track *ptr = (struct track *) malloc (sizeof(struct track));
			cv->mfocus->track->prev = ptr;
			cv->ft = ptr;
			

			//update all the other links
			ptr->view = cv;
			ptr->next = cv->mfocus->track;
			ptr->prev = NULL;
			ptr->c = cv->mfocus;
			ptr->size = cv->mfocus->track->size;
			cv->mfocus->track = ptr;
			
			
			//patch up the hole in mfocus->track
			if (cv->mfocus->prev != NULL) 
				cv->mfocus->prev->next = cv->mfocus->next;
			if (cv->mfocus->next != NULL)
				cv->mfocus->next->prev = cv->mfocus->prev;
						
			cv->mfocus->prev = NULL;
			cv->mfocus->next = NULL;
								
		};
				
	};
	if (cv->mfocus->prev != NULL) {
		cv->mfocus->size = cv->mfocus->prev->size;
	} else if (cv->mfocus->next != NULL) {
		cv->mfocus->size = cv->mfocus->next->size;
	};
			
};

void shift_main_focus(short int dir) {

	dir = convert_to_internal_dir(dir);

	if (dir == 1) {

		if (cv->mfocus->next != NULL) {
			cv->mfocus =  cv->mfocus->next;
		};
	} else if (dir == 2) { 
	
		if (cv->mfocus->prev != NULL) {
			cv->mfocus = cv->mfocus->prev;
		};
	} else if (dir == 3 && cv->mfocus->track->next != NULL) {
		
		struct cont *p;
			p = cv->mfocus->prev;
			int s = 0;
			int t = 0;

			while (p != NULL) {
				s += p->size;
				p = p->prev;
	
			};
			
			s += (cv->mfocus->size / 2);
			
			p = cv->mfocus->track->next->c;
			
			while (p->next != NULL) {
				t += p->size;
				if (t >= s) {break;};
				p = p->next;
			};
			
			cv->mfocus = p;
		
	} else if (dir == 4 && cv->mfocus->track->prev != NULL) {
		struct cont *p;
			p = cv->mfocus->prev;
			int s = 0;
			int t = 0;

			while (p != NULL) {
				s += p->size;
				p = p->prev;

			};
			
			s += (cv->mfocus->size / 2);
			
			p = cv->mfocus->track->prev->c;
			
			while (p->next != NULL) {
				t += p->size;
				if (t >= s) {break;};
				p = p->next;
			};
			cv->mfocus = p;
	};
};


struct view * find_view (int i) {
	//this will return a pointer to a give view
	//even if it must first create it
	
	//we have a dblly linked list of views
	//some views get a numbered index
		
	//i -2 means move forward -3 means backward
	
	struct view *v;
	
	if (i == -2) {
		//move forward
		if (cv->next != NULL) {
			return(cv->next);
		} else {
			//make cv->next
			v = make_view();
			cv->next = v;
			v->prev = cv;
			v->idx = (cv->idx + 1);
			
		//since there is no prev, it is obviously first
		
			
		};
	} else if (i == -3) {
		//move backward
		if (cv->prev != NULL) {
			return(cv->prev);
		} else {
			//make cv->prev
			v = make_view();
			v->next = cv;
			cv->prev = v;
			v->idx = (cv->idx - 1);
			fv = v;
		};
	} else if (i <= 9 && i >= 1) {
		
			v = fv;
			
			//the 2nd time i go to view 2 it returns v 1
			
			
			while (v->next != NULL && v->next->idx < i) {
				v = v->next;
			};
			
			if (v->next == NULL) {
				
				//we overshot, and ran out of views: make the view
				//or we are looking for fv
				if (i == v->idx) {
	
					return(v);
				};

				v->next = make_view();
				v->next->idx = i;
				v->next->prev = v;
				v = v->next;
				
			} else if (v->next->idx > i && v->idx < i) {
				
				//we overshot, but there are higher indexed views
				//make a view in front of v
	
				//store v->next
				struct view *v2 = v->next;
				v->next = make_view();
				v->next->idx = i;
				v->next->prev = v;
				v->next->next = v2;
				v2->prev = v->next;
				v = v->next;
				
			} else if (v->idx == i){
				;
			} else if (v->next->idx == i) {
				v = v->next;
			};
			
	};

	return(v);
	
};

void goto_view(struct view *v) {
	//this just unmaps the windows of the current view
	//sets cv
	//and maps the windows of the new cv
	
	if (v == NULL || v == cv) {return;};
	
	struct track *t = cv->ft;
	struct cont *c;
	while (t != NULL) {
		c = t->c;
		while (c != NULL) {
			XUnmapWindow(dpy,c->win->id);
			c = c->next;
		};
		t = t->next;
	};
	
	cv = v;
	t = v->ft;
	
	while (t != NULL) {
		c = t->c;
		while (c != NULL) {
			XMapWindow(dpy,c->win->id);
			c = c->next;
		};
		t = t->next;
	};
};


void move_to_view(struct view *v) {
	//move currenlty focused item
	
	if (v == NULL || v == cv) {return;};
	
	//remove it from the current view
	struct win *w = cv->mfocus->win;
	XUnmapWindow(dpy,w->id);
	remove_cont(cv->mfocus);
	
	//add it to the new view
	add_client_to_view(w, v);
};

//this is a quick simple version, may well need to be reworked

struct cont * id_to_cont(Window w) {

	struct track *t = cv->ft;
	struct cont *c;
	while (t != NULL) {
		c = t->c;
		while (c!= NULL) {
			if (c->win->id == w) {
				return (c);
			};
			c = c->next;
		};
		t = t->next;
	};
	return (NULL);
 
};

void resize (int dir) {
	
	if (cv->orientv == true) {
		switch (dir) {
			case 1:
				cv->mfocus->size -= 15;
			break; 
			case 2:
				cv->mfocus->track->size += 15;
			break;
			case 3:
				cv->mfocus->size += 15;
			break;
			case 4:
				cv->mfocus->track->size -= 15;
			break;
		};
		
	
	} else {
		switch (dir) {
			case 1:
				cv->mfocus->track->size += 15;
			break; 
			case 2:
				cv->mfocus->size += 15;
			break;
			case 3:
				cv->mfocus->track->size -= 15;
			break;
			case 4:
				cv->mfocus->size -= 15;
			break;
		};
		
	}
	
	
};

void layout() {
	//what mode are we in?
	
	//rather than do all this stack crap later, lets just set a variable stackh upfront, 0 if its hidden
	int stackheight;
	if (cv->showstack == false) {
		stackheight = 0;
	} else {
		//count items in stack
		struct stack_item *si = cv->stack;
		int i = 0;
		//how can this cause a segfault?
		//si must be garbage
		while (si != NULL) {
			i++;
			si = si->next;
		};
		stackheight = (i * 20); 
		if (i == 0) {
			stackheight = 3;
		};
	};
	
	//draw the stack 
	
	XClearWindow(dpy,stackid);
	if (stackheight != 0) {
		
		XMoveResizeWindow(dpy,stackid,0,(scrn_h - (stackheight)),scrn_w,(stackheight));
		XSync(dpy,false);
		
		struct stack_item *si = cv->stack;
		int i = 15;
		while (si != NULL) {
			GC gc;
			XGCValues xgcv;
			if (si == cv->sfocus) {
				xgcv.foreground = stack_focus_pix;
			} else {
				xgcv.foreground = stack_unfocus_pix;
			}; 
			
			gc = XCreateGC(dpy,stackid,GCForeground,&xgcv);
			
			XTextProperty wmname;
			XGetWMName(dpy,si->win->id,&wmname);	
			
			XDrawString(dpy,stackid,gc,3,i,wmname.value,wmname.nitems);	
			
			si = si->next;
			i += 20;
		}; 
	} else {
			XMoveResizeWindow(dpy,stackid,0,(scrn_h ),scrn_w,10);
	};
	XSync(dpy,false);
	
	if (cv->mfocus == NULL) {
		XSetInputFocus(dpy,root,None,CurrentTime);
	};
	
	if (cv->fs == true) {
		//draw mf fullscreen
		//make sure height is less stackheight
		int w = scrn_w;
		int h = scrn_h;
		h -= stackheight;
		
		XMoveResizeWindow(dpy,cv->mfocus->win->id,(0),(0),(w),(h));
		XRaiseWindow(dpy,cv->mfocus->win->id);
		XSetInputFocus(dpy,cv->mfocus->win->id,None,CurrentTime);
		XSync(dpy,false);
		
		
	} else {
		
		//first check that the tracks layout:
		struct track *curt = cv->ft;
		struct cont *curc = NULL;
		int target;
		int tot = 0;
		
		if (cv->orientv == true) {
			target = scrn_w;
		} else {
			if (cv->showstack == true) {
				//stackheight subtract from sreen_h and set target
				target = scrn_h - stackheight;
			} else {
				target = scrn_h;
			};
		};
		int nooftracks = 0;
		while (curt != NULL) {
			//check if the size is negligably small, requireing a reseize
			if (curt->size <= 5) {
				curt->size += 40;
			};
			tot += curt->size;
			curt = curt->next;
			nooftracks ++;
		};
		//compare tot to target, if they match go on 
		if (tot != target) {
			signed int delta = target - tot;
			 
			delta /= nooftracks;

			curt = cv->ft;
			while (curt != NULL) {
				
				curt->size += delta;
				curt = curt->next;
			};
		};
		//else calculate the difference, and adjust all tracks
			
		
		//second check that within each track the containers fit
		curt = cv->ft;
		
		if (cv->orientv != true) {
			target = scrn_w;
		} else {
			if (cv->showstack == true) {
				target = scrn_h - stackheight;
			} else {
				target = scrn_h;
			};
		}; 
		
		while (curt != NULL) {
			curc = curt->c;
			int tot = 0;	
			int noofconts = 0;
			while (curc != NULL) {
				//check for cont with negligable size and resize if necessary
				if (curc->size <= 5) {
					curc->size += 40;
				};
				
				noofconts ++;	
				tot += curc->size;
	
				curc = curc->next;
			};
			//if tot == target, do nothing
			//else calculate and distribute difference
			if (tot != target) {
				signed int delta = target - tot;
			 	if (noofconts != 0) {
					delta /= noofconts;
				} else {
					delta = 0;
				};
		
			
				curc = curt->c;
				while (curc != NULL) {
		
					curc->size += delta;
					curc = curc->next;
				};
			};
			
			curt = curt->next;
		};
		
		
		//walk and draw
		curt = cv->ft; //first track of view
		int x;
		int y;
		int h;
		int w;
		int offsett = 0;
		int offsetc = 0;
		
		while (curt != NULL) {
			offsetc = 0;
			curc = curt->c;
			while (curc != NULL) {
				//draw curc
				//set border:
				if (curc == cv->mfocus) {
			
				//set border
					XSetWindowBorder(dpy,curc->win->id,focus_pix);
					XSetInputFocus(dpy,curc->win->id,None,CurrentTime);
				
				} else {
					XSetWindowBorder(dpy,curc->win->id,unfocus_pix);
				};
				//place window
				//where do x, y, h, w come from?
				if (cv->orientv == true) {
					x = offsett;
					y = offsetc;
					w = curt->size;
					h = curc->size;
				} else {
					x = offsetc;
					y = offsett;
					w = curc->size;
					h = curt->size;
				};
			
				XMoveResizeWindow(dpy,curc->win->id,(x),(y),(w - 3),(h - 3));
				offsetc += curc->size;
				curc = curc->next;
			};
			offsett += curt->size;
			curt = curt->next;
		};
	};
	
};

int xerror(Display *d, XErrorEvent *e) {
	//get and print the error description for diagnostics:
	char buff[256];

	XGetErrorText(dpy, e->error_code, buff, 256);

	if (e->error_code == BadWindow) {
	
		forget_win((Window) e->resourceid);
	};
	gxerror = true;
	return(0);
};


int event_loop() {

	bool redraw;

	layout();
	XEvent ev;
	for (;;) {

		redraw = false; //this will get set to true if something gets changed onscreen
	do {
		XNextEvent(dpy, &ev);
	
		if (ev.type == MotionNotify && sloppy_focus == true) {
			
		if (cv->mfocus->win->id != ev.xmotion.window) {
				struct cont *f = id_to_cont(ev.xmotion.window);
				if (f != NULL) {
					cv->mfocus = f;

					redraw = true;
				} else {
					;
				};

			}; 
			
		} else if (ev.type == KeyPress) {
			
			//first find the keypress index from bindings[]
			int i = 0;
			
			//TODO this can cause a segfault
			//apparently i'm listening to more keys than i have recorded? (overrunning the array)
			
			while ((bindings[i].keycode != ev.xkey.keycode) || (bindings[i].mask != ev.xkey.state)) {
				i++;
			};
		

			switch (i) {
			
				//resize
				case 0:
					resize(4);
					redraw = true;
					break;
				case 1:
					resize(3);
					redraw = true;
					break;
				case 2:
					resize(1);
					redraw = true;
					break;
				case 3:
					resize(2);
					redraw = true;
					break;
				
				//move win to view 1-0 n m
				case 4:
					move_to_view(find_view(1));
					redraw = true;
					break;
				case 5:
					move_to_view(find_view(2));
					redraw = true;
					break;
				case 6:
					move_to_view(find_view(3));
					redraw = true;
					break;
				case 7:
					move_to_view(find_view(4));
					redraw = true;
					break;
				case 8:
					move_to_view(find_view(5));
					redraw = true;
					break;
				case 9:
					move_to_view(find_view(6));
					redraw = true;
					break;
				case 10:
					move_to_view(find_view(7));
					redraw = true;
					break;
				case 11:
					move_to_view(find_view(8));
					redraw = true;
					break;
				case 12:
					move_to_view(find_view(9));
					redraw = true;
					break;
				case 13:
				//	move_to_view(-3);
					redraw = true;

					break;
				case 14:
				//	move_to_view(-2);
					move_to_view(find_view(-3));
					redraw = true;

					break;
				case 15:
				//	move_to_view(-1);
					move_to_view(find_view(-2));
					redraw = true;

					break;
				//change view
				//internally views are 0 index
				//userside 1 indexed, w/0 pointing to the last
				case 16: //1 pressed, view 0
					goto_view(find_view(1));
					redraw = true;
					break;
				case 17:
					goto_view(find_view(2));
					redraw = true;
					break;
				case 18:
					goto_view(find_view(3));
					redraw = true;

					break;
				case 19:
					goto_view(find_view(4));
					redraw = true;

				break;
				case 20:
					goto_view(find_view(5));
					redraw = true;

				break;
				case 21:
					goto_view(find_view(6));
					redraw = true;

				break;
				case 22:
					goto_view(find_view(7));
					redraw = true;

				break;
				case 23:
					goto_view(find_view(8));
					redraw = true;

				break;
				case 24:
					goto_view(find_view(9));
					redraw = true;

				break;
				//goto last:
				case 25:
				//	goto_view(-3);
					redraw = true;

				break;
				//goto next/prev
				case 26:
					goto_view(find_view(-3));
					redraw = true;

				break;
				case 27:
					goto_view(find_view(-2));
					redraw = true;
				break;
				//shift window
				case 28:
					shift_window(4);
					redraw = true;
					break;
				case 29:
					shift_window(3);
					redraw = true;
					break;
				case 30:
					shift_window(1);
					redraw = true;
					break;
				case 31:
					shift_window(2);
					redraw = true;
					break;
				//toggle stack
				case 32:
				/*in the new version we are going to create this window the the view
				 * and destroy it with the view
				 * if it is not displayed, we just unmap it
				 * is this going to cause us problems with redrawing when the content changes?
				 * 
				 */
				
					if (cv->showstack == true) {
					//	XDestroyWindow(dpy,cur_view->stack_id);	
						cv->showstack = false;
					//	cur_view->layout->size = scrn_h;		
					} else {
						cv->showstack = true;
						//we have to make sure overrideredirect is set
						//args: display, parent,x,y,width,height,border_w,border_pix,background
						//these pixel values could be a problem, may have to define them
				//		cur_view->stack_id = XCreateSimpleWindow(dpy,root,0,(scrn_h-15),scrn_w,15,1,stack_unfocus_pix,stack_background_pix);
				//		XSetWindowAttributes att;
				//		att.override_redirect = true;
				//		XChangeWindowAttributes(dpy,cur_view->stack_id,CWOverrideRedirect,&att);
				//		XMapWindow(dpy,cur_view->stack_id);
					};
					
					redraw = true;
					break;
				//move to stack
				case 33:
					if (cv->mfocus == NULL) {break;};
					XUnmapWindow(dpy,cv->mfocus->win->id);	
					move_to_stack(cv->mfocus);
					
					redraw = true;
					break;
				//move to main
				case 34:
					move_to_main();
					//this was cv->mfocus ???
					XMapWindow(dpy,cv->mfocus->win->id);
				//	XSync(dpy,false);	
				
					redraw = true;
					break;
				//swap stack main
				//we are actually going to use this to flip the layout
				case 35:
					if (cv->orientv == true) {
						cv->orientv = false;
					} else {
						cv->orientv = true;
					};
					redraw = true;
					break;
				//swap stack up/down
				case 36:
					break;
				case 37:
					break;
				
				//shift main focus udrl
				case 38:
					shift_main_focus(4);
					redraw = true;
					break;
				case 39:
					shift_main_focus(3);
					redraw = true;
					break;
				case 40:
					shift_main_focus(1);
					redraw = true;
					break;
				case 41:
					shift_main_focus(2);
					redraw = true;
					break;
				//shift stack focus up down	
				case 42:
					shift_stack_focus(true);
					redraw = true;
					break;
				case 43:
					shift_stack_focus(false);
					redraw = true;
					break;
				//close win soft or hard
				case 44:
					printf("closing %6.0lx\n",cv->mfocus->win->id);
					Atom *prot = NULL;
					Atom *pp;
					int n, j;
					
						if (XGetWMProtocols(dpy, cv->mfocus->win->id, &prot, &n)) {
							for (j = 0, pp = prot; j < n; j++, pp++)
								if (*pp == wm_del_win) {
							
										XClientMessageEvent	cm;

										bzero(&cm, sizeof cm);
										cm.type = ClientMessage;
										cm.window = cv->mfocus->win->id;
										cm.message_type = wm_prot;
										cm.format = 32;
										cm.data.l[0] = wm_del_win;
										cm.data.l[1] = CurrentTime;
										XSendEvent(dpy, cv->mfocus->win->id, False, 0L, (XEvent *)&cm);
								} else {
									
									XDestroyWindow(dpy,cv->mfocus->win->id);
								};
 							if (prot)
								XFree(prot);
						};
					

					break;
				case 45:
					XKillClient(dpy,cv->mfocus->win->id);
					break;
				//run menu/xterm
				case 46:
					system("dmenu_run &");
					break;
				case 47:
					//spawn xterm
					system("x-terminal-emulator &");
					break;
				//fullscreen:
				case 48:
					if (cv->fs == true) {
						cv->fs = false;
					} else { 
						cv->fs = true;
					};
					redraw = true;
					break;
					
			};
			
		} else if (ev.type == ConfigureRequest) {
	
			//check whether we know about it
			//look at scrotwm 3198 
			XWindowChanges wc;
			wc.x = ev.xconfigurerequest.x;
			wc.y = ev.xconfigurerequest.y;
			wc.width = ev.xconfigurerequest.width;
			wc.height = ev.xconfigurerequest.height;
			wc.border_width = ev.xconfigurerequest.border_width;
			wc.sibling = ev.xconfigurerequest.above;
			wc.stack_mode = ev.xconfigurerequest.detail;
			XConfigureWindow(dpy,ev.xconfigurerequest.window,ev.xconfigurerequest.value_mask, &wc);
			
			add_client_to_view(add_win(ev.xconfigurerequest.window),cv);
			redraw = 1;

		} else if (ev.type == MapRequest) {
	
		
		} else if (ev.type == EnterNotify && sloppy_focus == true) {
	//		printf("Mouse entered win %6.0lx\n",ev.xcrossing.window);
			//set focus
		/*	struct box *f;
			f = id_to_box(ev.xcrossing.window);
			if (f != NULL) {
				cur_view->main_focus == f;
				redraw == true;
			}; */
			//do we give focus to windows we don't know about?
			//XSetInputFocus(dpy,cur_view->main_focus->win->id,None,CurrentTime);
			//redraw = true;
		} else if (ev.type == ReparentNotify) {
		//this and destroy notify need to be rewritten
		//of primary importance: 
		//we must ensure that we are checking all layouts! (otherwise swtiching desktops as we get a destroy notify
		//will leave null pointers)
		//also we need to make sure we implement all part of reparent
		//we should make a forget_win() func that both can call

			if (ev.xreparent.parent == root) {

				if (is_top_level(ev.xreparent.window) == true) {

					//add win, check whether it's mapped and add to view	
					//TODO
					//look at the startup routine in main() for how to do this

					redraw = true;
				} else {
					;
				};
			} else {
				forget_win(ev.xreparent.window);	
			};
			
		} else if (ev.type == DestroyNotify ) {

			forget_win(ev.xdestroywindow.window);
			redraw = true;
				
		} else if (ev.type == MapNotify && is_top_level(ev.xmap.window) == true) {
			//check whether it's in the layout, if not add it
			if (id_to_cont(ev.xmap.window) != NULL) {
				 //it's a dup
			} else {
				//see whether we know about the window
				struct win *w;
				w = first_win;
			
				while (w != NULL && w->id != ev.xmap.window) {
					w = w->next;
				};
				if (w == NULL) { //we don't have a record of it
					w = add_win(ev.xmap.window);
				};
				//finally add to layout
				add_client_to_view(w,cv);
				redraw = true;
		};
		
		} else if (ev.type == UnmapNotify ) {
			//don't think we should check is_top_level, since it may already have b
			//been destroyed
	//TODO XXX
			//not noting this seems to cause problems
			/*struct box *s;
			s = id_to_box(ev.xunmap.window);
			if (s == NULL ) {
				printf("can't unmap %6.0lx\n",ev.xunmap.window);
			} else {
				printf("removing box for win %6.0lx\n",s->win->id);
				//we don't want to move it to stack
				//move_to_stack(s);
				//we want to leave its wins entry
				//but remove its box
				//we also have to check whether it's focused
				if (s->win == cur_view->main_focus->win) {
					struct box *tmp;
					tmp = find_rel_win(true,true);
					if (tmp == NULL) {tmp = find_rel_win(true,false);};
					if (tmp == NULL) {tmp = find_rel_win(false,true);};
					if (tmp == NULL) {tmp = find_rel_win(false, false);}
					cur_view->main_focus = tmp;

					if (cur_view->main_focus != NULL) {
						printf("focus now set to %6.0lx\n",cur_view->main_focus->win->id);
					} else {
						printf("no windows\n");
					};
	
				};
				remove_box(s);
				//unless we caused this, we should check the window's original state 
				//before setting this
				redraw = true;
			};
			*/
		} else if (ev.type == CreateNotify && is_top_level(ev.xcreatewindow.window) ==true) {

			add_win(ev.xcreatewindow.window);
		} else if (ev.type == MappingNotify) {
			;
		};
	
	} while (XPending(dpy)); 
	
	if (redraw == true) {

		layout();
	};
	}; //end infinite loop


};


int main() {
	printf("\nRunning\n");
	
	dpy = XOpenDisplay(0);
	scrn_h = DisplayHeight(dpy,DefaultScreen(dpy));
	scrn_w = DisplayWidth(dpy,DefaultScreen(dpy));
	printf("Sreen dimensions: %d %d\n",scrn_h, scrn_w);
	
	//code this:
	//cv = make_view();

	//set some stuff
	if (root = DefaultRootWindow(dpy)) {
		printf("root is %6.0lx\n",root);
	} else {
		printf("faild to find root window\n");
		return(0);
	};
	
	//set colors:
	XColor color;
	//0 is screen number:
	//these values are shorts, 0 - 65535
	//focus color:
	color.red = 0;
	color.green = 0;
	color.blue = 65500;
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy,DefaultColormap(dpy,0),&color);
	focus_pix = color.pixel;
	//unfocus color
	color.red = 5000;
	color.green = 5000;
	color.blue = 5000;
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy,DefaultColormap(dpy,0),&color);
	unfocus_pix = color.pixel;
	//stack background:
	color.red = 100;
	color.green = 100;
	color.blue = 200;
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy,DefaultColormap(dpy,0),&color);
	stack_background_pix = color.pixel;
	//stack unfocus text:
	color.red = 60000;
	color.green = 60000;
	color.blue = 60000;
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy,DefaultColormap(dpy,0),&color);
	stack_unfocus_pix = color.pixel;
	//stack focus text:
	color.red = 0;
	color.green = 0;
	color.blue = 65500;
	color.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(dpy,DefaultColormap(dpy,0),&color);
	stack_focus_pix = color.pixel;
	
	
	//we have to do this after we get root
	bind_keys();
	
	//get the delwin atom
	wm_del_win = XInternAtom(dpy,"WM_DELETE_WINDOW",True);
	wm_prot = XInternAtom(dpy, "WM_PROTOCOLS", False);
	//to compensate for dumb programs
	work_around();
	 int i;
	 
	 cv = make_view();
	 fv = cv;
	 cv->idx = 1;
	
	//now we also need to get all already exisiting windows
	Window d1, d2, *wins = NULL;
	unsigned int no;
	XQueryTree(dpy,root,&d1,&d2,&wins,&no);
	struct win *t;
	printf("%d windows\n",no);
	for (i = 0 ; i<no; i++) {
		
		//we must make sure all these window are mapped 
		//before adding them to the first view
	//needs to be coded

		if (is_top_level(wins[i])) {
			
			t = add_win(wins[i]);
			XWindowAttributes att;
			XGetWindowAttributes(dpy,wins[i],&att);
			if (att.map_state != IsUnmapped) {
			//addendum
				add_client_to_view(t,cv);
			};
		};

	};
	//and set an event on root to get new ones
	if  (XSelectInput(dpy,root,SubstructureNotifyMask )) {
		printf("Now controlling root\n");
	} else {
		printf("Fail to control root, is there already a WM?");
		return(1);
	};
	XSync(dpy,False);

//make the stack window:
	stackid = XCreateSimpleWindow(dpy,root,0,(scrn_h-15),scrn_w,15,1,stack_unfocus_pix,stack_background_pix);
	XSetWindowAttributes att;
	att.override_redirect = true;
	XChangeWindowAttributes(dpy,stackid,CWOverrideRedirect,&att);
	XMapWindow(dpy,stackid);
	XSync(dpy,False);

	//we need to set a custom error handler, so we don't die on recoverable errors
	//code this:
	XSetErrorHandler(xerror);
	
	layout();
	//code:
	event_loop();
	return (0);
};
