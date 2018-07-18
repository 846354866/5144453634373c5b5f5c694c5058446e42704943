#include "input_event_manager.h"

#include "1_button.h"

const InputEventConfig_t InputEventConfig  = 
{
	/* Table to convert from PIO to input event ID*/
	{
		 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1,  4,  3,  2,  1,  5,  6, -1,  7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	},

	/* Masks for each PIO bank to configure as inputs */
	{ 0x00000001UL, 0x000002fcUL, 0x00000000UL },
	/* PIO debounce settings */
	4, 5
};

const InputActionMessage_t InputEventActions[] = 
{
	{
		VOL_PLUS,                               /* Input event bits */
		VOL_PLUS | VOL_MINUS,                   /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP,                   /* Message */
	},
	{
		VOL_MINUS,                              /* Input event bits */
		VOL_MINUS | VOL_PLUS,                   /* Input event mask */
		ENTER,                                  /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_DOWN,                 /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_3,                  /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_2,                  /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_1,                  /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD,                                   /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_HELD_4,                  /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD,                                   /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_HELD_FACTORY_RESET,          /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD_HELD,               /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		HELD,                                   /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD_HELD,                /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD,                                   /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_HELD_DFU,                    /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_6_SECOND,                /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		6000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FACTORY_RESET,               /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		1000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_1_SECOND,                /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD_HELD_RELEASE,       /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		8000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_8_SECOND,                /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		HELD_RELEASE,                           /* Action */
		500,                                    /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD_HELD_RELEASE,        /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		HELD_RELEASE,                           /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_POWER_OFF,                   /* Message */
	},
	{
		SW8,                                    /* Input event bits */
		SW8,                                    /* Input event mask */
		HELD_RELEASE,                           /* Action */
		3000,                                   /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_DFU,                         /* Message */
	},
	{
		VOL_PLUS | VOL_MINUS,                   /* Input event bits */
		VOL_PLUS | VOL_MINUS,                   /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP_DOWN,              /* Message */
	},
	{
		VOL_PLUS,                               /* Input event bits */
		VOL_PLUS,                               /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_UP_RELEASE,           /* Message */
	},
	{
		BACK,                                   /* Input event bits */
		BACK,                                   /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_BACKWARD,                    /* Message */
	},
	{
		FORWARD,                                /* Input event bits */
		FORWARD,                                /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_FORWARD,                     /* Message */
	},
	{
		VOL_MINUS,                              /* Input event bits */
		VOL_MINUS,                              /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_BUTTON_VOLUME_DOWN_RELEASE,         /* Message */
	},
	{
		SYS_CTRL,                               /* Input event bits */
		SYS_CTRL,                               /* Input event mask */
		RELEASE,                                /* Action */
		0,                                      /* Timeout */
		0,                                      /* Repeat */
		APP_MFB_BUTTON_PRESS,                   /* Message */
	},
};


