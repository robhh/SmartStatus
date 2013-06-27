#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "globals.h"



#define MY_UUID { 0x3B, 0xEF, 0x19, 0x30, 0x4E, 0xE8, 0x44, 0x9B, 0x89, 0x5E, 0x3C, 0x03, 0xAA, 0x37, 0x20, 0x86 }
PBL_APP_INFO(MY_UUID,
             "SmartStatus", "Robert Hesse",
             1, 0, /* App version */
             RESOURCE_ID_APP_ICON,
             APP_INFO_STANDARD_APP);

#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	8


AppMessageResult sm_message_out_get(DictionaryIterator **iter_out);
void reset_sequence_number();
char* int_to_str(int num, char *outbuf);
void sendCommand(int key);
void sendCommandInt(int key, int param);
void rcv(DictionaryIterator *received, void *context);
void dropped(void *context, AppMessageResult reason);
void select_single_click_handler(ClickRecognizerRef recognizer, Window *window);
void up_single_click_handler(ClickRecognizerRef recognizer, Window *window);
void down_single_click_handler(ClickRecognizerRef recognizer, Window *window);
void config_provider(ClickConfig **config, Window *window);
void battery_layer_update_callback(Layer *me, GContext* ctx);
void handle_status_appear(Window *window);
void handle_status_disappear(Window *window);
void handle_init(AppContextRef ctx);
void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t);
void handle_deinit(AppContextRef ctx);	
	

Window window;

TextLayer text_weather_cond_layer, text_weather_temp_layer;
TextLayer text_date_layer, text_time_layer;
TextLayer text_mail_layer, text_sms_layer, text_phone_layer, text_battery_layer;
Layer battery_layer;
BitmapLayer background_image, weather_image;

char string_buffer[STRING_LENGTH];
char weather_cond_str[STRING_LENGTH], weather_temp_str[5], sms_count_str[5], mail_count_str[5], phone_count_str[5];
int weather_img, batteryPercent;

HeapBitmap bg_image;
HeapBitmap weather_status_imgs[NUM_WEATHER_IMAGES];


const int WEATHER_IMG_IDS[] = {
  RESOURCE_ID_IMAGE_SUN,
  RESOURCE_ID_IMAGE_RAIN,
  RESOURCE_ID_IMAGE_CLOUD,
  RESOURCE_ID_IMAGE_SUN_CLOUD,
  RESOURCE_ID_IMAGE_FOG,
  RESOURCE_ID_IMAGE_WIND,
  RESOURCE_ID_IMAGE_SNOW,
  RESOURCE_ID_IMAGE_THUNDER
};




static uint32_t s_sequence_number = 0xFFFFFFFE;

AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_out_get(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
    return APP_MSG_OK;
}

void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_out_get(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_out_send();
    app_message_out_release();
}


char* int_to_str(int num, char *outbuf) {
	int digit, i=0, j=0;
	char buf[STRING_LENGTH];
	bool negative=false;
	
	if (num < 0) {
		negative = true;
		num = -1 * num;
	}
	
	for (i=0; i<STRING_LENGTH; i++) {
		digit = num % 10;
		if ((num==0) && (i>0)) 
			break;
		else
			buf[i] = '0' + digit;
		 
		num/=10;
	}
	
	if (negative)
		buf[i++] = '-';
	
	buf[i--] = '\0';
	
	
	while (i>=0) {
		outbuf[j++] = buf[i--];
	}
	
	outbuf[j] = '\0';
	
	return outbuf;
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, -1);
	app_message_out_send();
	app_message_out_release();	
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, param);
	app_message_out_send();
	app_message_out_release();	
}


void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;
	int *val;


	t=dict_find(received, SM_WEATHER_COND_KEY); 
	if (t!=NULL) {
		memcpy(weather_cond_str, t->value->cstring, strlen(t->value->cstring));
        weather_cond_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_cond_layer, weather_cond_str); 	
	}

	t=dict_find(received, SM_WEATHER_TEMP_KEY); 
	if (t!=NULL) {
		memcpy(weather_temp_str, t->value->cstring, strlen(t->value->cstring));
        weather_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_weather_temp_layer, weather_temp_str); 	
	}

	t=dict_find(received, SM_COUNT_MAIL_KEY); 
	if (t!=NULL) {
		memcpy(mail_count_str, t->value->cstring, strlen(t->value->cstring));
        mail_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_mail_layer, mail_count_str); 	
	}

	t=dict_find(received, SM_COUNT_SMS_KEY); 
	if (t!=NULL) {
		memcpy(sms_count_str, t->value->cstring, strlen(t->value->cstring));
        sms_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_sms_layer, sms_count_str); 	
	}

	t=dict_find(received, SM_COUNT_PHONE_KEY); 
	if (t!=NULL) {
		memcpy(phone_count_str, t->value->cstring, strlen(t->value->cstring));
        phone_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(&text_phone_layer, phone_count_str); 	
	}

	t=dict_find(received, SM_WEATHER_ICON_KEY); 
	if (t!=NULL) {
		bitmap_layer_set_bitmap(&weather_image, &weather_status_imgs[t->value->uint8].bmp);	  	
	}


	t=dict_find(received, SM_COUNT_BATTERY_KEY); 
	if (t!=NULL) {
		batteryPercent = t->value->uint8;
		layer_mark_dirty(&battery_layer);
		text_layer_set_text(&text_battery_layer, int_to_str(batteryPercent, string_buffer) ); 	
	}


}

void dropped(void *context, AppMessageResult reason){
	// DO SOMETHING WITH THE DROPPED REASON / DISPLAY AN ERROR / RESEND 
}



void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;



}


void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

	sendCommand(SM_OPEN_SIRI_KEY);
}


void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
  (void)recognizer;
  (void)window;

	text_layer_set_text(&text_weather_cond_layer, "Updating..." ); 	
	
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	
}


void config_provider(ClickConfig **config, Window *window) {
  (void)window;


  config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
//  config[BUTTON_ID_SELECT]->long_click.handler = (ClickHandler) select_long_click_handler;
//  config[BUTTON_ID_SELECT]->long_click.release_handler = (ClickHandler) select_long_release_handler;


  config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
//  config[BUTTON_ID_UP]->long_click.handler = (ClickHandler) up_long_click_handler;
//  config[BUTTON_ID_UP]->long_click.release_handler = (ClickHandler) up_long_release_handler;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
//  config[BUTTON_ID_DOWN]->long_click.handler = (ClickHandler) down_long_click_handler;
//  config[BUTTON_ID_DOWN]->long_click.release_handler = (ClickHandler) down_long_release_handler;

}

void battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(2, 2+16-(int)((batteryPercent/100.0)*16.0), 8, (int)((batteryPercent/100.0)*16.0)), 0, GCornerNone);
	
}


void handle_status_appear(Window *window)
{
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
}

void handle_status_disappear(Window *window)
{
	sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
}


void handle_init(AppContextRef ctx) {
	(void)ctx;

	window_init(&window, "Window Name");
	window_set_window_handlers(&window, (WindowHandlers) {
	    .appear = (WindowHandler)handle_status_appear,
	    .disappear = (WindowHandler)handle_status_disappear
	});

	window_stack_push(&window, true /* Animated */);
	window_set_fullscreen(&window, true);

	resource_init_current_app(&APP_RESOURCES);

	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
		heap_bitmap_init(&weather_status_imgs[i], WEATHER_IMG_IDS[i]);
	}
	
	heap_bitmap_init(&bg_image, RESOURCE_ID_IMAGE_BACKGROUND);

	bitmap_layer_init(&background_image, GRect(0, 0, 144, 168));
	layer_add_child(&window.layer, &background_image.layer);
	bitmap_layer_set_bitmap(&background_image, &bg_image.bmp);


	text_layer_init(&text_weather_cond_layer, GRect(5, 80, 47, 40));
	text_layer_set_text_alignment(&text_weather_cond_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_weather_cond_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_cond_layer, GColorClear);
	text_layer_set_font(&text_weather_cond_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(&window.layer, &text_weather_cond_layer.layer);
	text_layer_set_text(&text_weather_cond_layer, "Updating"); 	


	text_layer_init(&text_weather_temp_layer, GRect(92, 82, 47, 40));
	text_layer_set_text_alignment(&text_weather_temp_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_weather_temp_layer, GColorWhite);
	text_layer_set_background_color(&text_weather_temp_layer, GColorClear);
	text_layer_set_font(&text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	layer_add_child(&window.layer, &text_weather_temp_layer.layer);
	text_layer_set_text(&text_weather_temp_layer, "-°"); 	


	text_layer_init(&text_date_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_date_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_date_layer, GColorWhite);
	text_layer_set_background_color(&text_date_layer, GColorClear);
	layer_set_frame(&text_date_layer.layer, GRect(0, 45, 144, 30));
	text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
	layer_add_child(&window.layer, &text_date_layer.layer);


	text_layer_init(&text_time_layer, window.layer.frame);
	text_layer_set_text_alignment(&text_time_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_time_layer, GColorWhite);
	text_layer_set_background_color(&text_time_layer, GColorClear);
	layer_set_frame(&text_time_layer.layer, GRect(0, -5, 144, 50));
	text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
	layer_add_child(&window.layer, &text_time_layer.layer);


	text_layer_init(&text_mail_layer, GRect(8, 147, 30, 48));
	text_layer_set_text_alignment(&text_mail_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_mail_layer, GColorWhite);
	text_layer_set_background_color(&text_mail_layer, GColorClear);
	text_layer_set_font(&text_mail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(&window.layer, &text_mail_layer.layer);
	text_layer_set_text(&text_mail_layer, "-"); 	


	text_layer_init(&text_sms_layer, GRect(46, 147, 30, 48));
	text_layer_set_text_alignment(&text_sms_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_sms_layer, GColorWhite);
	text_layer_set_background_color(&text_sms_layer, GColorClear);
	text_layer_set_font(&text_sms_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(&window.layer, &text_sms_layer.layer);
	text_layer_set_text(&text_sms_layer, "-"); 	

	text_layer_init(&text_phone_layer, GRect(85, 147, 23, 48));
	text_layer_set_text_alignment(&text_phone_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_phone_layer, GColorWhite);
	text_layer_set_background_color(&text_phone_layer, GColorClear);
	text_layer_set_font(&text_phone_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(&window.layer, &text_phone_layer.layer);
	text_layer_set_text(&text_phone_layer, "-"); 	

	text_layer_init(&text_battery_layer, GRect(110, 147, 30, 48));
	text_layer_set_text_alignment(&text_battery_layer, GTextAlignmentCenter);
	text_layer_set_text_color(&text_battery_layer, GColorWhite);
	text_layer_set_background_color(&text_battery_layer, GColorClear);
	text_layer_set_font(&text_battery_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	layer_add_child(&window.layer, &text_battery_layer.layer);
	text_layer_set_text(&text_battery_layer, "-");


	layer_init(&battery_layer, GRect(118, 131, 11, 19));
	battery_layer.update_proc = &battery_layer_update_callback;
	layer_add_child(&window.layer, &battery_layer);

	batteryPercent = 100;
	layer_mark_dirty(&battery_layer);

	weather_img = 0;

	bitmap_layer_init(&weather_image, GRect(52, 81, 40, 40));
	layer_add_child(&window.layer, &weather_image.layer);
	bitmap_layer_set_bitmap(&weather_image, &weather_status_imgs[0].bmp);

	window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);

}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

  (void)ctx;

  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;

  string_format_time(date_text, sizeof(date_text), "%a, %b %e", t->tick_time);
  text_layer_set_text(&text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);

}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;

	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	heap_bitmap_deinit(&weather_status_imgs[i]);
	}

  	heap_bitmap_deinit(&bg_image);

	
}


void pbl_main(void *params) {

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
	.messaging_info = {
		.buffer_sizes = {
			.inbound = 124,
			.outbound = 256
		},
		.default_callbacks.callbacks = {
			.in_received = rcv,
			.in_dropped = dropped
		}
	},
	.tick_info = {
	  .tick_handler = &handle_minute_tick,
	  .tick_units = MINUTE_UNIT
	}

  };
  app_event_loop(params, &handlers);
}
