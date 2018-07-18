#include "input_event_manager.h"

#include "1_button.h"

const InputEventConfig_t InputEventConfig  = 
{
	/* Table to convert from PIO to input event ID*/
	{
		 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1,  3,  2,  0, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1,  5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	},

	/* Masks for each PIO bank to configure as inputs */
	{ 0x00000001UL, 0x00f00000UL, 0x00000010UL },
	/* PIO debounce settings */
	4, 5
};

const InputActionMessage_t InputEventActions[] = 
{
	{
		SW6,                                    /* Input event bits */
		SW6 | SW3,                              /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP,                   /* Message */
	},
	{
		SW3,                                    /* Input event bits */
		SW3 | SW6,                              /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_DOWN,                 /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD,                                   /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_3,                  /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD,                                   /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_2,                  /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_1,                  /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD,                                   /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_HELD_1,                      /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD,                                   /* Action */
		15000,                                  /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_HELD_FACTORY_RESET,          /* Message */
	},
	{
		SW4,                                    /* Input event bits */
		SW4,                                    /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD_HELD,               /* Message */
	},
	{
		SW7,                                    /* Input event bits */
		SW7,                                    /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD_HELD,                /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD,                                   /* Action */
		12000,                                  /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_HELD_DFU,                    /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_6_SECOND,                /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		15000,                                  /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FACTORY_RESET,               /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_1_SECOND,                /* Message */
	},
	{
		SW4,                                    /* Input event bits */
		SW4,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD_HELD_RELEASE,       /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_8_SECOND,                /* Message */
	},
	{
		SW7,                                    /* Input event bits */
		SW7,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD_HELD_RELEASE,        /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		12000,                                  /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_DFU,                         /* Message */
	},
	{
		SW5,                                    /* Input event bits */
		SW5,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_POWER_OFF,                   /* Message */
	},
	{
		SW6 | SW3,                              /* Input event bits */
		SW6 | SW3,                              /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP_DOWN,              /* Message */
	},
	{
		SW6,                                    /* Input event bits */
		SW6,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP_RELEASE,           /* Message */
	},
	{
		SW4,                                    /* Input event bits */
		SW4,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD,                    /* Message */
	},
	{
		SW7,                                    /* Input event bits */
		SW7,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD,                     /* Message */
	},
	{
		SW3,                                    /* Input event bits */
		SW3,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_DOWN_RELEASE,         /* Message */
	},
	{
		SW2,                                    /* Input event bits */
		SW2,                                    /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_PRESS,                   /* Message */
	},
};


