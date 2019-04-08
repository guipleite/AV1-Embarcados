#include "asf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ioport.h"

#define YEAR        2018
#define MONTH      3
#define DAY         19
#define WEEK        12
#define HOUR        0
#define MINUTE      0
#define SECOND      0
uint32_t hour, minu, seg;

//butao 1 oled
#define EBUT1_PIO PIOD //start EXT 9 PD28
#define EBUT1_PIO_ID 16
#define EBUT1_PIO_IDX 28
#define EBUT1_PIO_IDX_MASK (1u << EBUT1_PIO_IDX)
//butao 2 oled
#define EBUT2_PIO PIOA //pause  Ext 4 PA19 PA = 10
#define EBUT2_PIO_ID 10
#define EBUT2_PIO_IDX 19
#define EBUT2_PIO_IDX_MASK (1u << EBUT2_PIO_IDX)
//butao 3 oled
#define EBUT3_PIO PIOC //sei la EXT 3 PC31
#define EBUT3_PIO_ID 12 // piod ID
#define EBUT3_PIO_IDX 31
#define EBUT3_PIO_IDX_MASK (1u << EBUT3_PIO_IDX)

struct ili9488_opt_t g_ili9488_display_opt;

volatile Bool wheel;
volatile Bool but_m_freq;
volatile Bool but_stop;

volatile Bool f_rtt_alarme = false;

void wheel_callback(void){
	wheel = true;
}
void but_m_freq_callback(void){
	but_m_freq = true;
}
void but_stop_callback(void){
	but_stop = true;
}

void init(){
	board_init();
	/* Insert system clock initialization code here (sysclk_init()). */
	sysclk_init();
	
	// configura botoes do oled
	pmc_enable_periph_clk(EBUT1_PIO_ID);
	pmc_enable_periph_clk(EBUT2_PIO_ID);
	pmc_enable_periph_clk(EBUT3_PIO_ID);
	// configura botoes do oled como input
	pio_set_input(EBUT1_PIO,EBUT1_PIO_IDX_MASK,PIO_DEFAULT);
	pio_pull_up(EBUT1_PIO,EBUT1_PIO_IDX_MASK,PIO_PULLUP);
	pio_set_input(EBUT2_PIO,EBUT2_PIO_IDX_MASK,PIO_DEFAULT);
	pio_pull_up(EBUT2_PIO,EBUT2_PIO_IDX_MASK,PIO_PULLUP);
	pio_set_input(EBUT3_PIO,EBUT3_PIO_IDX_MASK,PIO_DEFAULT);
	pio_pull_up(EBUT3_PIO,EBUT3_PIO_IDX_MASK,PIO_PULLUP);
	
	// Ativa interrup??o
	pio_enable_interrupt(EBUT1_PIO, EBUT1_PIO_IDX_MASK);
	pio_enable_interrupt(EBUT2_PIO, EBUT2_PIO_IDX_MASK);
	pio_enable_interrupt(EBUT3_PIO, EBUT3_PIO_IDX_MASK);

	// Configura NVIC para receber interrupcoes do PIO do botao
	
	NVIC_EnableIRQ(EBUT1_PIO_ID);
	NVIC_SetPriority(EBUT1_PIO_ID, 0); // Prioridade 4
	NVIC_EnableIRQ(EBUT2_PIO_ID);
	NVIC_SetPriority(EBUT2_PIO_ID, 0); // Prioridade 4
	NVIC_EnableIRQ(EBUT3_PIO_ID);
	NVIC_SetPriority(EBUT3_PIO_ID, 0); // Prioridade 4
	
	// Configura interrup??o no pino referente ao botao e associa
	// fun??o de callback caso uma interrup??o for gerada
	// a fun??o de callback ? a: but_callback()
	pio_handler_set(EBUT1_PIO,
					EBUT1_PIO_ID,
					EBUT1_PIO_IDX_MASK,
					PIO_IT_FALL_EDGE,
					wheel_callback);
	pio_handler_set(EBUT2_PIO,
					EBUT2_PIO_ID,
					EBUT2_PIO_IDX_MASK,
					PIO_IT_FALL_EDGE,
					but_m_freq_callback);
	pio_handler_set(EBUT3_PIO,
					EBUT3_PIO_ID,
					EBUT3_PIO_IDX_MASK,
					PIO_IT_FALL_EDGE,
					but_stop_callback);
	
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	
}

static void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_TOMATO));
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, 120-1);
	ili9488_draw_filled_rectangle(0, 360, ILI9488_LCD_WIDTH-1, 480-1);	
}

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(RTC, YEAR, MONTH, DAY, WEEK);
	rtc_set_time(RTC, HOUR, MINUTE, SECOND);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(RTC,  RTC_IER_ALREN);

}

void pin_toggle(Pio *pio, uint32_t mask);
void io_init(void);
static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		f_rtt_alarme = true;                  // flag RTT alarme
	}
}

static float get_time_rtt(){
	uint ul_previous_time = rtt_read_timer_value(RTT);
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));
	
	rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}

int calc_Vel(int N,double dT){
	return 3.6*2*3.14*0.325*N/dT;
}
int calc_dist(int N){
	return 2*3.14*0.325*N;
}

void clear_LCD(int a, int b){
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0, a, ILI9488_LCD_WIDTH-1, b);
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
}
int main(void){
	// array para escrita no LCD
	uint8_t stingLCD[256];
	
	/* Initialize the board. */
	ioport_init();
	init();
	RTC_init();
    /* Inicializa e configura o LCD */
	configure_lcd();
	clear_LCD(300,316);
	
	int x = 0;
	int x_tot = 0;
	Bool flag_time = true;
	Bool flag_dist = false;
	Bool flag_vel = false;
	
	rtc_get_time(RTC, &hour, &minu, &seg);
	int t_time = seg;
	
    f_rtt_alarme = true;
	
	int n_alarm = 0;

	while (1) {
		rtc_get_time(RTC, &hour, &minu, &seg);
		
		if(f_rtt_alarme){
			 uint16_t pllPreScale = (int) (((float) 32768) / 4);
			 uint32_t irqRTTvalue  = 4;
			 
			 // reinicia RTT para gerar um novo IRQ
			 RTT_init(pllPreScale, irqRTTvalue);
			 f_rtt_alarme = false;
			 n_alarm++;
			 
			 clear_LCD(250,315);
		
			 sprintf(stingLCD, "Tempo Percorrido:%d :%d :%d", hour,minu,seg);
			 ili9488_draw_string(10, 300, stingLCD);
			 
			 if(n_alarm%4==0){
				clear_LCD(125,170);
				sprintf(stingLCD,"Velocidade Media:%d km/h" , calc_Vel(x,4));
				ili9488_draw_string(10, 150, stingLCD);
				sprintf(stingLCD,"Dist. Percorrida:%d m" , calc_dist(x_tot));
				ili9488_draw_string(5, 125, stingLCD);
				x=0;
			 }
		}
			
		if(but_m_freq){
			x++;
			x_tot+=x;
			clear_LCD(200,230);
			
			sprintf(stingLCD, "Rotacoes %d", x_tot);
			ili9488_draw_string(100, 200, stingLCD);
			
			but_m_freq = false;
		}
		else if(but_stop){
			but_stop = false;
		}
		else if(wheel){
			wheel=false;
		}
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	
	}
	return 0;
}
