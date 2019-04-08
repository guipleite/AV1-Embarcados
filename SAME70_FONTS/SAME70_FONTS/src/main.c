#include "asf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ioport.h"

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

volatile Bool but_p_freq;
volatile Bool but_m_freq;
volatile Bool but_stop;

void but_p_freq_callback(void){
	but_p_freq = true;
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

	// Configura interrup??o no pino referente ao botao e associa
	// fun??o de callback caso uma interrup??o for gerada
	// a fun??o de callback ? a: but_callback()
	pio_handler_set(EBUT1_PIO,
	EBUT1_PIO_ID,
	EBUT1_PIO_IDX_MASK,
	PIO_IT_FALL_EDGE,
	but_p_freq_callback);
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

void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	/* Configura o PMC */
	
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrup?c?o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup?c?o no TC canal 0 */
	/* Interrup??o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}
void show_LCD(int x){
	uint8_t stingLCD[256];

	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(10, 200, ILI9488_LCD_WIDTH-1, 250);
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
	sprintf(stingLCD, "soma %d", x);
	ili9488_draw_string(100, 200, stingLCD);
}
void TC1_Handler(void){
	volatile uint32_t ul_dummy;

	ul_dummy = tc_get_status(TC0, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	/** Muda o estado do LED */
	show_LCD(7);
}



void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);

int main(void)
{
	// array para escrita no LCD
	uint8_t stingLCD[256];
	
	/* Initialize the board. */
	
	ioport_init();
	init();
	
	/* Initialize the UART console. */
	//configure_console();

    /* Inicializa e configura o LCD */
	configure_lcd();

    /* Escreve na tela Computacao Embarcada 2018 */
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
	ili9488_draw_filled_rectangle(0, 300, ILI9488_LCD_WIDTH-1, 315);
	ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
	
	sprintf(stingLCD, "Computacao Embarcada %d", 2019);
	ili9488_draw_string(10, 300, stingLCD);
	int x = 0;
	while (1) {
		//C_init(TC0, ID_TC1, 1, 1);
		
		if(but_stop){
			ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
			ili9488_draw_filled_rectangle(10, 200, ILI9488_LCD_WIDTH-1, 250);
			ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
			sprintf(stingLCD, "soma %d", 1201);
			ili9488_draw_string(100, 200, stingLCD);
			but_stop = false;
		}
		else if(but_p_freq){
		
			//TC_init(TC0, ID_TC1, 1, 0.25);

			but_p_freq=false;
			
		}
		else if(but_m_freq){
			ili9488_set_foreground_color(COLOR_CONVERT(COLOR_WHITE));
			ili9488_draw_filled_rectangle(10, 200, ILI9488_LCD_WIDTH-1, 250);
			ili9488_set_foreground_color(COLOR_CONVERT(COLOR_BLACK));
			sprintf(stingLCD, "soma %d", 1234);
			ili9488_draw_string(100, 200, stingLCD);
			but_m_freq=false;
			
		}
		
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
		
	}
	return 0;
}
