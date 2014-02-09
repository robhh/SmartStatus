#include <pebble.h>
#include "globals.h"

static Window *window;

#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	9

typedef enum {WEATHER_LAYER, CALENDAR_LAYER, MUSIC_LAYER, NUM_LAYERS} AnimatedLayers;


static PropertyAnimation *ani_out, *ani_in;


static TextLayer *text_weather_cond_layer, *text_weather_temp_layer;
static TextLayer *text_date_layer, *text_time_layer;
static TextLayer *text_mail_layer, *text_sms_layer, *text_phone_layer, *text_battery_layer;
static TextLayer *calendar_date_layer, *calendar_text_layer;
static TextLayer *music_artist_layer, *music_song_layer;

static Layer *battery_layer, *pebble_battery_layer;
static BitmapLayer *background_image, *weather_image;

static Layer *status_layer, *animated_layer[3];

static int active_layer;

static char string_buffer[STRING_LENGTH];
static char weather_cond_str[STRING_LENGTH], weather_temp_str[6], sms_count_str[5], mail_count_str[5], phone_count_str[5];
static int weather_img, batteryPercent, batteryPblPercent;

static char calendar_date_str[STRING_LENGTH], calendar_text_str[STRING_LENGTH];
static char music_artist_str[STRING_LENGTH], music_title_str[STRING_LENGTH];


GBitmap *bg_image;
GBitmap *weather_status_imgs[NUM_WEATHER_IMAGES];


const int WEATHER_IMG_IDS[] = {
  RESOURCE_ID_IMAGE_SUN,
  RESOURCE_ID_IMAGE_RAIN,
  RESOURCE_ID_IMAGE_CLOUD,
  RESOURCE_ID_IMAGE_SUN_CLOUD,
  RESOURCE_ID_IMAGE_FOG,
  RESOURCE_ID_IMAGE_WIND,
  RESOURCE_ID_IMAGE_SNOW,
  RESOURCE_ID_IMAGE_THUNDER,
  RESOURCE_ID_IMAGE_DISCONNECT
};


static uint32_t s_sequence_number = 0xFFFFFFFE;

AppMessageResult sm_message_out_get(DictionaryIterator **iter_out) {
    AppMessageResult result = app_message_outbox_begin(iter_out);
    if(result != APP_MSG_OK) return result;
    dict_write_int32(*iter_out, SM_SEQUENCE_NUMBER_KEY, ++s_sequence_number);
    if(s_sequence_number == 0xFFFFFFFF) {
        s_sequence_number = 1;
    }
    return APP_MSG_OK;
}

void reset_sequence_number() {
    DictionaryIterator *iter = NULL;
    app_message_outbox_begin(&iter);
    if(!iter) return;
    dict_write_int32(iter, SM_SEQUENCE_NUMBER_KEY, 0xFFFFFFFF);
    app_message_outbox_send();
}


void sendCommand(int key) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, -1);
	app_message_outbox_send();
}


void sendCommandInt(int key, int param) {
	DictionaryIterator* iterout;
	sm_message_out_get(&iterout);
    if(!iterout) return;
	
	dict_write_int8(iterout, key, param);
	app_message_outbox_send();
}


void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;


  // TODO: Only update the date when it's changed.
  strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
  text_layer_set_text(text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(text_time_layer, time_text);
}




void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;
	int *val;



	t=dict_find(received, SM_WEATHER_COND_KEY); 
	if (t!=NULL) {
		memcpy(weather_cond_str, t->value->cstring, strlen(t->value->cstring));
        weather_cond_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_weather_cond_layer, weather_cond_str); 	
	}

	t=dict_find(received, SM_WEATHER_TEMP_KEY); 
	if (t!=NULL) {
		memcpy(weather_temp_str, t->value->cstring, strlen(t->value->cstring));
		APP_LOG(APP_LOG_LEVEL_DEBUG, "STRING LENGTH: %d", strlen(t->value->cstring));

        weather_temp_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_weather_temp_layer, weather_temp_str); 	
	}

	t=dict_find(received, SM_COUNT_MAIL_KEY); 
	if (t!=NULL) {
		memcpy(mail_count_str, t->value->cstring, strlen(t->value->cstring));
        mail_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_mail_layer, mail_count_str); 	
	}

	t=dict_find(received, SM_COUNT_SMS_KEY); 
	if (t!=NULL) {
		memcpy(sms_count_str, t->value->cstring, strlen(t->value->cstring));
        sms_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_sms_layer, sms_count_str); 	
	}

	t=dict_find(received, SM_COUNT_PHONE_KEY); 
	if (t!=NULL) {
		memcpy(phone_count_str, t->value->cstring, strlen(t->value->cstring));
        phone_count_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(text_phone_layer, phone_count_str); 	
	}

	t=dict_find(received, SM_WEATHER_ICON_KEY); 
	if (t!=NULL) {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[t->value->uint8]);	  	
	}


	t=dict_find(received, SM_COUNT_BATTERY_KEY); 
	if (t!=NULL) {
		batteryPercent = t->value->uint8;
		layer_mark_dirty(battery_layer);
		
		snprintf(string_buffer, sizeof(string_buffer), "%d", batteryPercent);
		text_layer_set_text(text_battery_layer, string_buffer); 	
	}


	t=dict_find(received, SM_STATUS_CAL_TIME_KEY); 
	if (t!=NULL) {
		memcpy(calendar_date_str, t->value->cstring, strlen(t->value->cstring));
        calendar_date_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(calendar_date_layer, calendar_date_str); 	
	}

	t=dict_find(received, SM_STATUS_CAL_TEXT_KEY); 
	if (t!=NULL) {
		memcpy(calendar_text_str, t->value->cstring, strlen(t->value->cstring));
        calendar_text_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(calendar_text_layer, calendar_text_str); 	
	}


	t=dict_find(received, SM_STATUS_MUS_ARTIST_KEY); 
	if (t!=NULL) {
		memcpy(music_artist_str, t->value->cstring, strlen(t->value->cstring));
        music_artist_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(music_artist_layer, music_artist_str); 	
	}

	t=dict_find(received, SM_STATUS_MUS_TITLE_KEY); 
	if (t!=NULL) {
		memcpy(music_title_str, t->value->cstring, strlen(t->value->cstring));
        music_title_str[strlen(t->value->cstring)] = '\0';
		text_layer_set_text(music_song_layer, music_title_str); 	
	}


}


static void select_click_handler(ClickRecognizerRef recognizer, void *context) {

	ani_out = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(0, 77, 144, 45), &GRect(-144, 77, 144, 45));
	animation_schedule((Animation*)ani_out);


	active_layer = (active_layer + 1) % (NUM_LAYERS);


	ani_in = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(144, 77, 144, 45), &GRect(0, 77, 144, 45));
	animation_schedule((Animation*)ani_in);

}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
	sendCommand(SM_OPEN_SIRI_KEY);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	text_layer_set_text(text_weather_cond_layer, "Updating..." ); 	
	
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
}

static void click_config_provider(void *context) {


  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);


}

void reset() {
	
	text_layer_set_text(text_weather_cond_layer, "Updating..."); 	
	
}

void battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(15-(int)((batteryPercent/100.0)*15.0), 0, 15, 7), 0, GCornerNone);
	
}

void pebble_battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(15-(int)((batteryPblPercent/100.0)*15.0), 0, 15, 7), 0, GCornerNone);
	
}

static void window_load(Window *window) {


}

static void window_unload(Window *window) {
//  text_layer_destroy(text_layer);
}

static void window_appear(Window *window)
{
	
  
	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	
}


static void window_disappear(Window *window)
{
	sendCommandInt(SM_SCREEN_EXIT_KEY, STATUS_SCREEN_APP);
	
}

void reconnect(void *data) {
	reset();

	sendCommandInt(SM_SCREEN_ENTER_KEY, STATUS_SCREEN_APP);
	
}

void bluetoothChanged(bool connected) {

	if (connected) {
		app_timer_register(5000, reconnect, NULL);
	} else {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[NUM_WEATHER_IMAGES-1]);
		vibes_double_pulse();
	}
	
}


void batteryChanged(BatteryChargeState batt) {
	
	batteryPblPercent = batt.charge_percent;
	layer_mark_dirty(battery_layer);
	
}

static void init(void) {
  window = window_create();
  window_set_fullscreen(window, true);

  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
	.appear = window_appear,
	.disappear = window_disappear
  });
  const bool animated = true;
  window_stack_push(window, animated);


	//init weather images
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	weather_status_imgs[i] = gbitmap_create_with_resource(WEATHER_IMG_IDS[i]);
	}
	
  	bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);


  	Layer *window_layer = window_get_root_layer(window);

	//init background image
  	GRect bg_bounds = layer_get_frame(window_layer);

	background_image = bitmap_layer_create(bg_bounds);
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image));
	bitmap_layer_set_bitmap(background_image, bg_image);


		animated_layer[WEATHER_LAYER] = layer_create(GRect(0, 77, 144, 45));
		layer_add_child(window_layer, animated_layer[WEATHER_LAYER]);

		text_weather_cond_layer = text_layer_create(GRect(5, 2, 47, 40));
		text_layer_set_text_alignment(text_weather_cond_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_weather_cond_layer, GColorWhite);
		text_layer_set_background_color(text_weather_cond_layer, GColorClear);
		text_layer_set_font(text_weather_cond_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(animated_layer[WEATHER_LAYER], text_layer_get_layer(text_weather_cond_layer));
		text_layer_set_text(text_weather_cond_layer, "Updating"); 	


		text_weather_temp_layer = text_layer_create(GRect(92, 4, 47, 40));
		text_layer_set_text_alignment(text_weather_temp_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_weather_temp_layer, GColorWhite);
		text_layer_set_background_color(text_weather_temp_layer, GColorClear);
		text_layer_set_font(text_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
		layer_add_child(animated_layer[WEATHER_LAYER], text_layer_get_layer(text_weather_temp_layer));
		text_layer_set_text(text_weather_temp_layer, "-°"); 	


		text_date_layer = text_layer_create(bg_bounds);
		text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_date_layer, GColorWhite);
		text_layer_set_background_color(text_date_layer, GColorClear);
		layer_set_frame(text_layer_get_layer(text_date_layer), GRect(0, 44, 144, 30));
		text_layer_set_font(text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
		layer_add_child(window_layer, text_layer_get_layer(text_date_layer));


		text_time_layer = text_layer_create(bg_bounds);
		text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_time_layer, GColorWhite);
		text_layer_set_background_color(text_time_layer, GColorClear);
		layer_set_frame(text_layer_get_layer(text_time_layer), GRect(0, -6, 144, 50));
		text_layer_set_font(text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
		layer_add_child(window_layer, text_layer_get_layer(text_time_layer));



		animated_layer[CALENDAR_LAYER] = layer_create(GRect(144, 77, 144, 45));
		layer_add_child(window_layer, animated_layer[CALENDAR_LAYER]);

		calendar_date_layer = text_layer_create(GRect(6, 0, 132, 21));
		text_layer_set_text_alignment(calendar_date_layer, GTextAlignmentLeft);
		text_layer_set_text_color(calendar_date_layer, GColorWhite);
		text_layer_set_background_color(calendar_date_layer, GColorClear);
		text_layer_set_font(calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_date_layer));
		text_layer_set_text(calendar_date_layer, "No Upcoming"); 	


		calendar_text_layer = text_layer_create(GRect(6, 15, 132, 28));
		text_layer_set_text_alignment(calendar_text_layer, GTextAlignmentLeft);
		text_layer_set_text_color(calendar_text_layer, GColorWhite);
		text_layer_set_background_color(calendar_text_layer, GColorClear);
		text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
		layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_text_layer));
		text_layer_set_text(calendar_text_layer, "Appointment");

	//	layer_set_hidden(&animated_layer[CALENDAR_LAYER], true);



		animated_layer[MUSIC_LAYER] = layer_create(GRect(144, 77, 144, 45));
		layer_add_child(window_layer, animated_layer[MUSIC_LAYER]);

		music_artist_layer = text_layer_create(GRect(6, 0, 132, 21));
		text_layer_set_text_alignment(music_artist_layer, GTextAlignmentLeft);
		text_layer_set_text_color(music_artist_layer, GColorWhite);
		text_layer_set_background_color(music_artist_layer, GColorClear);
		text_layer_set_font(music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
		layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_artist_layer));
		text_layer_set_text(music_artist_layer, "Artist"); 	


		music_song_layer = text_layer_create(GRect(6, 15, 132, 28));
		text_layer_set_text_alignment(music_song_layer, GTextAlignmentLeft);
		text_layer_set_text_color(music_song_layer, GColorWhite);
		text_layer_set_background_color(music_song_layer, GColorClear);
		text_layer_set_font(music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
		layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_song_layer));
		text_layer_set_text(music_song_layer, "Title");



		status_layer = layer_create(GRect(0, 130, 144, 48));
		layer_add_child(window_layer, status_layer);


		text_mail_layer = text_layer_create(GRect(5, 18, 32, 48));
		text_layer_set_text_alignment(text_mail_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_mail_layer, GColorWhite);
		text_layer_set_background_color(text_mail_layer, GColorClear);
		text_layer_set_font(text_mail_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
		layer_add_child(status_layer, text_layer_get_layer(text_mail_layer));
		text_layer_set_text(text_mail_layer, "-"); 	


		text_sms_layer = text_layer_create(GRect(44, 18, 30, 48));
		text_layer_set_text_alignment(text_sms_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_sms_layer, GColorWhite);
		text_layer_set_background_color(text_sms_layer, GColorClear);
		text_layer_set_font(text_sms_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
		layer_add_child(status_layer, text_layer_get_layer(text_sms_layer));
		text_layer_set_text(text_sms_layer, "-"); 	

		text_phone_layer = text_layer_create(GRect(78, 18, 23, 48));
		text_layer_set_text_alignment(text_phone_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_phone_layer, GColorWhite);
		text_layer_set_background_color(text_phone_layer, GColorClear);
		text_layer_set_font(text_phone_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
		layer_add_child(status_layer, text_layer_get_layer(text_phone_layer));
		text_layer_set_text(text_phone_layer, "-"); 	

		text_battery_layer = text_layer_create(GRect(105, 18, 30, 48));
		text_layer_set_text_alignment(text_battery_layer, GTextAlignmentCenter);
		text_layer_set_text_color(text_battery_layer, GColorWhite);
		text_layer_set_background_color(text_battery_layer, GColorClear);
		text_layer_set_font(text_battery_layer,  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
		layer_add_child(status_layer, text_layer_get_layer(text_battery_layer));
		text_layer_set_text(text_battery_layer, "-");


		battery_layer = layer_create(GRect(112, 13, 15, 7));
		layer_set_update_proc(battery_layer, battery_layer_update_callback);
		layer_add_child(status_layer, battery_layer);

		batteryPercent = 100;
		layer_mark_dirty(battery_layer);

		pebble_battery_layer = layer_create(GRect(112, 0, 15, 7));
		layer_set_update_proc(pebble_battery_layer, pebble_battery_layer_update_callback);
		layer_add_child(status_layer, pebble_battery_layer);
	
		BatteryChargeState pbl_batt = battery_state_service_peek();
		batteryPblPercent = pbl_batt.charge_percent;
		layer_mark_dirty(pebble_battery_layer);


		if (bluetooth_connection_service_peek()) {
			weather_img = 0;
		} else {
			weather_img = NUM_WEATHER_IMAGES - 1;
		}

		weather_image = bitmap_layer_create(GRect(52, 2, 40, 40));
		layer_add_child(animated_layer[WEATHER_LAYER], bitmap_layer_get_layer(weather_image));
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[weather_img]);

		active_layer = WEATHER_LAYER;

	  	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

		bluetooth_connection_service_subscribe(bluetoothChanged);
		battery_state_service_subscribe(batteryChanged);


}

static void deinit(void) {
	

	
	animation_destroy((Animation*)ani_in);
	animation_destroy((Animation*)ani_out);
	
	text_layer_destroy(text_weather_cond_layer);
	text_layer_destroy(text_weather_temp_layer);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
	text_layer_destroy(text_mail_layer);
	text_layer_destroy(text_sms_layer);
	text_layer_destroy(text_phone_layer);
	text_layer_destroy(text_battery_layer);
	text_layer_destroy(calendar_date_layer);
	text_layer_destroy(calendar_text_layer);
	text_layer_destroy(music_artist_layer);
	text_layer_destroy(music_song_layer);
	layer_destroy(battery_layer);
	layer_destroy(pebble_battery_layer);
	bitmap_layer_destroy(background_image);
	bitmap_layer_destroy(weather_image);
	layer_destroy(status_layer);
	
	
	
	for (int i=0; i<NUM_LAYERS; i++) {
		layer_destroy(animated_layer[i]);
	}
	
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	gbitmap_destroy(weather_status_imgs[i]);
	}
	
	gbitmap_destroy(bg_image);

  	
	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	
  window_destroy(window);
}

int main(void) {
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum() );
	app_message_register_inbox_received(rcv);
	
  	init();


  	app_event_loop();
	app_message_deregister_callbacks();

  deinit();
}
