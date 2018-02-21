/*
    ZEsarUX  ZX Second-Emulator And Released for UniX
    Copyright (C) 2013 Cesar Hernandez Bano

    This file is part of ZEsarUX.

    ZEsarUX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*

El sonido en tres canales es producido por el AY-3-8912, un cir~uito de sonido muy popu-
lar; este dispositivo controla tambi n las puertas RS232/MIDI y AUX.
Las dos Puertas serie so1o pueden ser controladas por programa. El +2A no incluye soft-
ware para el ~ontrol de la puerta AUX, el cual deberi ser ~estionado por e~ pr~~rama de
usuario. En cambio, la puerta RS23~/MIDI es controlada Plenamentc por +3 BASIC.
La manera en ue ~rabaja el AY-3-8912 es bastante ~ompleja; se recomienda a quienes
~ sientan tentados a experimentar que consulten la ho~a de datos del AY-3-8912. No obs-
tante, la siguiente informaci~n deberia ser suficiente para empezar.
El circuito de sonido contiene dieciseis registros; para seleccionarlos, primero se escribe
el numero de registro en la puerta de escritura de direcciones, FFFDh (65533), y despues
lee el valor del registro (en la misma direccion) o se escribe en la direccion de escritura
de registros de datos, BFFDh (49149). Una vez seleccionado un registro, se puede realizar
cualquier numero de operaciones de lectura o escritura de datos. S~1o habr~ que volver
escribir en la puerta de escritura de direcciones cuando se necesite seleccio~ar otro registro.
La frecuencia de reloj basica de este circuito es 1.7734 MHz (con precisi~n del 0.01~~o).
Los registros hacen lo siguiente:
RO � Ajuste fino del tono, canal A
R1 � Ajuste aproximado del tono, canal A-
R2 � Ajuste fino del tono, canal B
R3 � Ajuste aproximado del tono, canal B
R4 � Ajuste fino del tono, canal C
R5 � Ajuste aproximado del tono, canal C

El tono de cada canal es un valor de 12 bits que se forma combinando los bits D3-DO
del registro de ajuste aproximado y los bits D7-DO del registro de ajuste fino. La uni-
dad b~sica del tono es la frecuencia de reloj ~ividida por 16 (es decir, 110.83 KHz).
Como el contador es de 12 bits, se puede g ~erar frecuencias de 27 Hz a 110 KHz.

R6 � Control del generador de ruido, D4-DO
El periodo del generador de ruido se toma contando los cinco bits inferiores del regis-
tro de ruido cada periodo del reloj de sonido dividido por 16.

R7 � Control del mezclador y de E/S
D7 No utilizado
D6 1=puerta de entrada, 0=puerta de salida
D5 Ruido en el canal C
D4 Ruido en el canal B
D3 Ruido en el canal A
D2 Tono en el canal C
D1 Tono en el canal B
DO Tono en el canal A
Seccion 30. Informaci~n de referencia
309
Este registro controla la mezcla de ruido y tono para cada canal y la direc i~n
puerta de E/S de o~ho bits. Un cero en un bit de mezcla indica que la funci~n
a~tivada.

R8 � Control de amplitud del ca~al A
R9 � Control de amplitud del canal B
RA � Control de amplitud del canal C
D4
l=utilizar generador de envolvente
O=utilizar el valor de D3-DO como amplitud
D3-DO Amplitud
Estos tres registros controlan la amplitud de cada canal y si esta debe ser modul~h
o no por los registros de envolvente.

RB � Ajuste aproximado del periodo de envolvente
RC � Ajuste fino del periodo de envolvente
Los valores de ocho bits de RB y RC se combinan para producir un numero de 16
bits que se cuenta en unidades de 256 por el periodo del relo~ de sonido. La~ frecuea-
cias de envolvente Pueden estar entre 0.1 Hz 6 KHz.

RD � Control de envolventes
D3 Continua
D2 Ataque
Dl Alternada
DO Sostenida
El diagrama de las formas de envolvente (Seccion 19 de este capitulo) da una ilustraci~n
gr
fica de los posibles estados de este registro.

Si est~ conectada la unidad de disquete externa, su control lo realiza el circuito controla-
dor ~PD765A del interfaz externo. Tal como explicamos en la Secci~n 23, el registro dc
datos de este di~positivn est en la direcci~n 3FFDh (16381); el re~istro de estado se en-
cuentra en 2FFDh (12285). Est~ dispositivo es muy complejo, por lo ue no se debe expe-
. rimenta~ con l si no se conoce perfectamente su funcionamiento (consultese la ho a de
datos del fabricante).
La puerta de impresora paralelo (Centronics) es basicamente un 'latch' de 8 bits (74273)
cuya direcci~n es OFFDh (4093). La se~al STROBE para la impresora es generada Por la
ULA, Y est~ accesible en el bit 4 dc la direc~.i~n 1FFDh (8189). El estado de la senal BUSY
procedente de la impresora puede ser leido en el bit 0 de la direccion OFFDh (4093).
310



*/

#include <stdio.h>
#include <math.h>


#include "ay38912.h"
#include "cpu.h"
#include "audio.h"
#include "debug.h"
#include "joystick.h"

//Indica si esta presente el chip o no
z80_bit ay_chip_present;


//Por defecto la frecuencia del spectrum
int ay_chip_frequency=FRECUENCIA_SPECTRUM_AY;

//#define FRECUENCIA_SPECTRUM_AY 1773400
//#define FRECUENCIA_CPC_AY      1000000
//#define FRECUENCIA_ZX81_AY     1625000



//Valores de volumen
char volume_table[16]={0,0,1,1,
			   1,2,2,3,
			   4,6,8,10,
			   14,16,20,24};


//Este es bit enviado cuando tanto tono como ruido son 0.... Esto permite speech, como en chase hq
//si lo ponemos a 0, no se oye nada
z80_bit ay_speech_enabled;

z80_bit ay_envelopes_enabled;

//una onda oscila 2 veces de signo en su frecuencia
#define TEMP_MULTIPLICADOR 1




//valores de 16 bits con signo
//tempp  short sine_table[FRECUENCIA_CONSTANTE_NORMAL_SONIDO];

short sine_table[FRECUENCIA_CONSTANTE_NORMAL_SONIDO];

//tabla suficientemente grande como para que al aumentar la frecuencia del sonido no se salga de rango
//soportar hasta cpu 1000% (10 veces mayor)
//short sine_table[15600*10];



//z80_bit turbosound_enabled={0};

int total_ay_chips=1;

//Chip de sonido activo (0 o 1)
int ay_chip_selected=0;

//
//Variables que dependen del chip activo
//
//16 BYTES Contenido de los registros del chip de sonido
z80_byte ay_3_8912_registros[MAX_AY_CHIPS][16];

//Ultimo registro seleccionado por el puerto 65533
z80_byte ay_3_8912_registro_sel[MAX_AY_CHIPS];

//frecuencia de canal de envelope
int freq_envelope[MAX_AY_CHIPS];

//contador de canal de ruido .... (FRECUENCIA_CONSTANTE_NORMAL_SONIDO/freq_ruido)
int contador_envelope[MAX_AY_CHIPS];

int ciclo_envolvente[MAX_AY_CHIPS];

int ciclo_envolvente_10_14_signo[MAX_AY_CHIPS];

//frecuencia de cada canal
int freq_tono_A[MAX_AY_CHIPS],freq_tono_B[MAX_AY_CHIPS],freq_tono_C[MAX_AY_CHIPS];

//contador de cada canal... (FRECUENCIA_CONSTANTE_NORMAL_SONIDO/freq_tono)
int contador_tono_A[MAX_AY_CHIPS],contador_tono_B[MAX_AY_CHIPS],contador_tono_C[MAX_AY_CHIPS];

//ultimo valor enviado para cada canal, valores con signo:
short ultimo_valor_tono_A[MAX_AY_CHIPS];
short ultimo_valor_tono_B[MAX_AY_CHIPS];
short ultimo_valor_tono_C[MAX_AY_CHIPS];

char ultimo_valor_envolvente[MAX_AY_CHIPS];

//frecuencia de canal de ruido
int freq_ruido[MAX_AY_CHIPS];

//contador de canal de ruido .... (FRECUENCIA_CONSTANTE_NORMAL_SONIDO/freq_ruido)
int contador_ruido[MAX_AY_CHIPS];

//ultimo valor enviado para canal de ruido. valor con signo:
short ultimo_valor_ruido[MAX_AY_CHIPS];

//valor randomize
z80_int randomize_noise[MAX_AY_CHIPS];

//
//Fin variables que dependen del chip activo
//




void return_envelope_name(int value,char *string)
{

//Hol-Alt-Ata-Con
//Hold,Alternate,Attack,Continue
	sprintf (string,"%s-%s-%s-%s",
		( (value&8) ? "Con" : "   "),
		( (value&4) ? "Att" : "   "),
		( (value&2) ? "Alt" : "   "),
		( (value&1) ? "Hol" : "   ")
	);
}


//Si hay que autoactivar chip AY en el caso que alguien lo use (lectura o escritura en el puerto AY)
z80_bit autoenable_ay_chip={1};

void init_chip_ay(void)
{


	//inicializamos el chip aunque este desactivado
	//if (ay_chip_present.v==0) return;

	debug_printf (VERBOSE_INFO,"Initializing AY Chip");

	ay_chip_selected=0;


	//resetear valores de cada chip
	int chip;

	for (chip=0;chip<MAX_AY_CHIPS;chip++) {

		//resetear valores de puertos de sonido
		int r;
		for (r=0;r<16;r++) ay_3_8912_registros[chip][r]=255;

		ciclo_envolvente[chip]=0;

		ciclo_envolvente_10_14_signo[chip]=+1;

		//ultimo valor enviado para cada canal, valores con signo:
		ultimo_valor_tono_A[chip]=+32767;
		ultimo_valor_tono_B[chip]=+32767;
		ultimo_valor_tono_C[chip]=+32767;

		ultimo_valor_envolvente[chip]=0;

		ultimo_valor_ruido[chip]=+32767;
	}

int i;



// Onda cuadrada
	for (i=0;i<FRECUENCIA_CONSTANTE_NORMAL_SONIDO/2;i++) {
		sine_table[i]=+32767;
	}
	for (;i<FRECUENCIA_CONSTANTE_NORMAL_SONIDO;i++) {
		sine_table[i]=-32767;
	}

	//Establecemos frecuencia
	if (MACHINE_IS_CPC) ay_chip_frequency=FRECUENCIA_CPC_AY;
	else if (MACHINE_IS_ZX8081) ay_chip_frequency=FRECUENCIA_ZX81_AY;
	else ay_chip_frequency=FRECUENCIA_SPECTRUM_AY;

	debug_printf (VERBOSE_INFO,"Setting AY chip frequency to %d HZ",ay_chip_frequency);




//Onda senoidal. activar -lm en proceso de compilacion
/*
float sineval,radians;
	for (i=0;i<FRECUENCIA_CONSTANTE_NORMAL_SONIDO;i++) {
		radians=i;
		radians=radians*6.28318530718;

		radians=radians/((float)(FRECUENCIA_CONSTANTE_NORMAL_SONIDO));
		sineval=sin(radians);
		sine_table[i]=32767*sineval;

		debug_printf (VERBOSE_DEBUG,"i=%d radians=%f sine=%f value=%d",i,radians,sineval,sine_table[i]);
	}

*/


}

/*
void turbosound_disable(void)
{
	debug_printf (VERBOSE_INFO,"Disabling Turbosound");
	ay_chip_selected=0;
	turbosound_enabled.v=0;
}

void turbosound_enable(void)
{
	debug_printf (VERBOSE_INFO,"Enabling Turbosound");
	ay_chip_selected=0;
	turbosound_enabled.v=1;
	init_chip_ay();
}
*/

void set_total_ay_chips(int total)
{

	if (total>MAX_AY_CHIPS || total<1) cpu_panic("Invalid value for ay chips");

	ay_chip_selected=0;
	total_ay_chips=total;
}

void ay_randomize(int chip)
{
/*
;Seguimos la misma formula RND del spectrum:
;0..1 -> n=(75*(n+1)-1)/65536
;0..65535 -> n=65536/(75*(n+1)-1)
generar_random_noise:
*/

        int resultado;
        int r;

        r=randomize_noise[chip];

        resultado=(75*(r+1)-1);

        randomize_noise[chip]=resultado & 0xFFFF;

        //printf ("randomize_noise: %d\n",randomize_noise);


}



//Generar salida aleatoria
void ay_chip_valor_aleatorio(int chip)
{

ay_randomize(chip);

/*
          ;Generar +1 o -1
          ;0..32767 -> +1
          ;32768..65535 -> -1
  */

	if (randomize_noise[chip]<32768) ultimo_valor_ruido[chip]=+32767;
	else ultimo_valor_ruido[chip]=-32767;

	//printf ("Cambio ruido a : %d\n",ultimo_valor_ruido);

}

char da_output_envolvente(int chip)
{
	return ultimo_valor_envolvente[chip];
}

//Devuelve la salida del canal indicado, devuelve valor con signo
char da_output_canal(z80_byte mascara,short ultimo_valor_tono,z80_byte volumen,int chip)
{

	//valor con signo
	char valor8;
	int valor;

/*
COMMENT !
    The noise and tone output of a channel is combined in the mixer in the
    following way:

        Output_A = (Tone_A OR Ta) AND (Noise OR Na)

    Here Tone_A is the binary output of tone generator A, and Noise is the
    binary output of the noise generator.  Note that setting both Ta and Na
    to 1 produces a constant 1 as output.  Also note that setting both Ta
    and Na to 0 produces bursts of noise and half-periods of constant
    output 0.
( Tone_a OR 0 ) AND ( Noise_a OR 0 )= Tone_a AND Noise_a
( Tone_a OR 0 ) AND ( Noise_a OR 1 )= Tone_a
( Tone_a OR 1 ) AND ( Noise_a OR 0 )= Noise_a
( Tone_a OR 1 ) AND ( Noise_a OR 1 )= 1
!
*/

	z80_bit tone,noise;

//	printf ("ultimo_valor_tono: %d\n",ultimo_valor_tono);

	tone.v=!(ay_3_8912_registros[chip][7] & mascara & 7);
	noise.v=!(ay_3_8912_registros[chip][7] & mascara & (8+16+32));

	if (tone.v==1 && noise.v==0)  {
		valor=ultimo_valor_tono;
		silence_detection_counter=0;
	}

	else if (tone.v==0 && noise.v==1)  {
                valor=ultimo_valor_ruido[chip];
                silence_detection_counter=0;
        }

	else if (tone.v==1 && noise.v==1)  {
		/*
		 Also note that setting both Ta
    and Na to 0 produces bursts of noise and half-periods of constant
    output 0.
*/
		//Valor combinado ruido y tono
		//en version 1.0 este /2 no estaba, era un error, por tanto se generaba al final un volumen mayor de lo normal,
		//cosa que podia hacer que el valor final del sonido cambiase de signo,
		//provocando ruido mas alto de lo normal
		valor=(ultimo_valor_ruido[chip]+ultimo_valor_tono)/2;

                silence_detection_counter=0;
		//printf ("tone y noise. valor:%d\n",valor);

        }

	else {
		//Canales desactivados
		//Parece que deberiamos devolver 0, pero no, devuelve 1
		//esto permite generar sintetizacion de voz/sonido, etc en juegos como Chase HQ por ejemplo, Dizzy III
		//valor=1;
		valor=ay_speech_enabled.v*32767;
	}



        if (volumen & 16) {
		//printf ("Hay envolvente activo tipo : %d\n",ay_3_8912_registros[13]&15);
		if (ay_envelopes_enabled.v==1) volumen=da_output_envolvente(chip);

		else volumen=15;
	}

	volumen=volumen & 15; //Evitar valores de volumen fuera de rango que vengan de los registros de volumen

	//if (volumen>15) printf ("  Error volumen >15 : %d\n",volumen);
        valor=valor*volume_table[volumen];
	valor=valor/32767;
	valor8=valor;
	//printf ("valor final tono: %d\n",valor8);

	//if (valor8>24) printf ("valor final tono: %d\n",valor8);
	//if (valor8<-24) printf ("valor final tono: %d\n",valor8);

	return valor8;


}

//Devuelve el sonido de salida del chip de los 3 canales
char da_output_ay(void)
{


        //char valor_enviar_ay=0;
        int valor_enviar_ay=0;
	if (ay_chip_present.v==1) {

		//Hacerlo para cada chip
		int chips=ay_retorna_numero_chips();

		int i;

		for (i=0;i<chips;i++) {

			valor_enviar_ay +=da_output_canal(1+8,ultimo_valor_tono_A[i],ay_3_8912_registros[i][8],i);
			valor_enviar_ay +=da_output_canal(2+16,ultimo_valor_tono_B[i],ay_3_8912_registros[i][9],i);
			valor_enviar_ay +=da_output_canal(4+32,ultimo_valor_tono_C[i],ay_3_8912_registros[i][10],i);


		}

		//Dividir valor restante entre numero de chips
		valor_enviar_ay /=chips;
	}


	/*
	if (valor_enviar_ay==0x10) {
		printf ("A %d B %d C %d\n",da_output_canal(1+8,ultimo_valor_tono_A,ay_3_8912_registros[8]),da_output_canal(2+16,ultimo_valor_tono_B,ay_3_8912_registros[9]),da_output_canal(4+32,ultimo_valor_tono_C,ay_3_8912_registros[10]) );
	}
	*/

	return valor_enviar_ay;

}

char devuelve_volumen_ciclo_envolvente()
{

/*

Formas de envolventes:

0,1,2,3  \___________

4,5,6,7  /-----------

8        \|\|\|\|\|\|

9        \___________

10       \/\/\/\/\/\/

11       \|----------

12       /|/|/|/|/|/|

13       /-----------

14       /\/\/\/\/\/\

15       /|__________

formas 10 y 14, cuando cambian de subida o bajada (abajo del envolvente o arriba) se repite el ultimo valor pico, es decir, en la 10 los volumenes son
15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 15 14 13 12 11 ...

y en la 14 son:
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 0 1 2 3 4 5 ...

no hay que hacer un caso especial para esa "repeticion" del valor donde cambia la onda, tal y como esta hecha la funcion,
ese valor ya se repite por si solo

*/
	char volumen;

	//ciclos que han finalizado y van a 0
	if (ciclo_envolvente[ay_chip_selected]==256) {
		return 0;
	}

	//ciclos que han finalizado y van a 1
	if (ciclo_envolvente[ay_chip_selected]==512) {
		return 15;
	}


	z80_byte tipo_envolvente=ay_3_8912_registros[ay_chip_selected][13]&15;
	switch (tipo_envolvente) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 8:
		case 9:
		case 11:
			volumen=15-ciclo_envolvente[ay_chip_selected];
			break;

		case 4:
		case 5:
		case 6:
		case 7:
		case 12:
		case 13:
		case 15:
			volumen=ciclo_envolvente[ay_chip_selected];
			break;

		case 10: /*    \./  \./    El . significa un ciclo adicional repitiendo el 0      */
			if (ciclo_envolvente_10_14_signo[ay_chip_selected]==+1) volumen=15-ciclo_envolvente[ay_chip_selected];
			else volumen=ciclo_envolvente[ay_chip_selected];
			break;
		case 14: /*    /.\  /.\    El . significa un ciclo adicional repitiendo el 15     */
                        if (ciclo_envolvente_10_14_signo[ay_chip_selected]==+1) volumen=ciclo_envolvente[ay_chip_selected];
                        else volumen=15-ciclo_envolvente[ay_chip_selected];
			break;

		default:
		//Aqui no deberia llegar nunca
		volumen=15;
		//debug_printf (VERBOSE_ERR,"Envelope type %d not implemented",tipo_envolvente);
		break;
	}


	//debug_printf (VERBOSE_DEBUG,"Envelope Cycle: %d volume: %d envelope type: %d",ciclo_envolvente,volumen,tipo_envolvente);

	//Siguiente ciclo
	ciclo_envolvente[ay_chip_selected]++;

	if (ciclo_envolvente[ay_chip_selected]==16) {

  	  switch (tipo_envolvente) {
	  //envolventes que se van a 0
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 9:
		case 15:
				//debug_printf (VERBOSE_DEBUG,"End envelope cycle. Going to 0");
				ciclo_envolvente[ay_chip_selected]=256;
		break;

	  //envolventes que se van a 1
		case 11:
		case 13:
				//debug_printf (VERBOSE_DEBUG,"End envelope cycle. Going to 1");
                                ciclo_envolvente[ay_chip_selected]=512;
		break;


	  //envolventes que se repiten
		default:

                                        //debug_printf (VERBOSE_DEBUG,"End envelope cycle. Going to the beginning");
                                        ciclo_envolvente[ay_chip_selected]=0;
					if (tipo_envolvente==10 || tipo_envolvente==14) {
						//envolventes en v
						ciclo_envolvente_10_14_signo[ay_chip_selected]=-ciclo_envolvente_10_14_signo[ay_chip_selected];
					}

		break;

	  }

	}


	return volumen;
}


//Calcular e invertir , si conviene, salida de cada canal
void ay_chip_siguiente_ciclo_siguiente(int chip)
{

if (ay_chip_present.v==0) return;

/*
                int r;
                for (r=0;r<15;r++)
                        printf ("R%d=%x ",r,ay_3_8912_registros[r]);
*/

                        //actualizamos contadores de frecuencias
                        ultimo_valor_tono_A[chip]=sine_table[contador_tono_A[chip]];
                        contador_tono_A[chip] +=freq_tono_A[chip];
                        if (contador_tono_A[chip]>=FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
                                contador_tono_A[chip] -=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
                        }

                        ultimo_valor_tono_B[chip]=sine_table[contador_tono_B[chip]];
                        contador_tono_B[chip] +=freq_tono_B[chip];
                        if (contador_tono_B[chip]>=FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
                                contador_tono_B[chip] -=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
                        }

                        ultimo_valor_tono_C[chip]=sine_table[contador_tono_C[chip]];
                        contador_tono_C[chip] +=freq_tono_C[chip];
                        if (contador_tono_C[chip]>=FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
                                contador_tono_C[chip] -=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
                        }


                        contador_ruido[chip] +=freq_ruido[chip];
                        if (contador_ruido[chip]>=FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
                                contador_ruido[chip] -=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
                                ay_chip_valor_aleatorio(chip);
                                //printf ("Conmutar ruido\n");
                        }


 			//Esto se ejecuta aunque desactivemos el envelope por linea de comandos... pero da igual, al final no se escuchara el envelope
                                contador_envelope[chip] +=freq_envelope[chip];

                        //*10 dado que hemos de ser capaz de generar frecuencias de 0.1
                        //*16 dado que hay 16 divisiones en cada pulso de envolvente
                        if (contador_envelope[chip]>=FRECUENCIA_CONSTANTE_NORMAL_SONIDO*10/16) {
                                contador_envelope[chip] -=FRECUENCIA_CONSTANTE_NORMAL_SONIDO*10/16;
                                ultimo_valor_envolvente[chip]=devuelve_volumen_ciclo_envolvente();
                        }


}



/*int ay_retorna_numero_chips(void)
{
	int chips=1;
        if (turbosound_enabled.v) chips++;
	return chips;
}*/


int ay_retorna_numero_chips(void)
{

	return total_ay_chips;

}

void ay_chip_siguiente_ciclo(void)
{

	int chips=ay_retorna_numero_chips();
	int j;
	for (j=0;j<chips;j++) {
		ay_chip_siguiente_ciclo_siguiente(j);
	}

}

int temp_gunstick_electron=1;
//leer puerto
z80_byte in_port_ay(z80_byte puerto_h)
{

        if (puerto_h==0xFF) {

		int r=ay_3_8912_registro_sel[ay_chip_selected] & 15;
		//evitamos valores fuera de rango

		if (r==14) {
			//debug_printf (VERBOSE_INFO,"registro chip ay: %d",r);
	               //gunstick
        	       if (gunstick_emulation==GUNSTICK_AYCHIP) {

                	        z80_byte acumulado=255;
                        	if (mouse_left!=0) {

	                            acumulado &=(255-32);

        	                    if (!gunstick_view_electron()) acumulado &=(255-16);
        	                    //if (!gunstick_view_electron()) acumulado |=temp_gunstick_electron;


                	        }

				//si no pulsado boton
				else acumulado=(255-16);

                        	ay_3_8912_registros[ay_chip_selected][14]=acumulado;

				//printf ("temp. valor aychip gunstick : %d\n",acumulado);

        	        }

		}

		return ay_3_8912_registros[ay_chip_selected][r];
        }

	return 255;
}


//Calcular contadores de incremento
//void establece_frecuencia_tono(z80_byte indice, int *freq_tono, int *contador_tono)
void establece_frecuencia_tono(z80_byte indice, int *freq_tono)
{

	int freq_temp;
	freq_temp=ay_3_8912_registros[ay_chip_selected][indice]+256*(ay_3_8912_registros[ay_chip_selected][indice+1] & 0x0F);
        //printf ("Valor freq_temp : %d\n",freq_temp);
        freq_temp=freq_temp*16;


	//controlamos divisiones por cero
	if (!freq_temp) freq_temp++;

        *freq_tono=FRECUENCIA_AY/freq_temp;

	//printf ("Valor freq_tono : %d\n",*freq_tono);

	/* Pruebas notas
	Octava 4. Nota C Valor registros: 424. Freq_tono=261 hz ok
	          Nota D Valor registros: 377. Freq_tono=293 hz ok

	Maximo valor en registro=12 bit=4095. 4095*16=65520
	FRECUENCIA_AY=1773400
	1773400/65520=27 Hz

	Minimo valor en registro=0
	1773400/(0*16) infinito

	Si es 1 por ejemplo, 1*16=16
	1773400/16= 110.837 KHz

	*/


	//freq_tono realmente tiene frecuencia*2... dice cada cuando se conmuta de signo
	//esto ya no hace falta con la tabla... multiplicador=1
	*freq_tono=(*freq_tono)*TEMP_MULTIPLICADOR;

        if (*freq_tono>FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
		//debug_printf (VERBOSE_DEBUG,"Frequency tone %d out of range",(*freq_tono)/TEMP_MULTIPLICADOR);
		*freq_tono=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
	}

	//si la frecuencia del tono es exactamente igual a la del sonido, pasara que siempre el valor de retorno
	//sera el primer valor en el array de sine_table.. seguramente +1 (32767)
	//alteramos un poco el valor
	if ( (*freq_tono)==FRECUENCIA_CONSTANTE_NORMAL_SONIDO) (*freq_tono)=FRECUENCIA_CONSTANTE_NORMAL_SONIDO-10;

	//de logica deberia resetear esto a 0.... pero si lo hago distorsiona el sonido
	//mejor dejar el contador con el valor actual
	//temp
	//*contador_tono=0;

        //printf ("Valor incremento frecuencia canal : %d contador_tono : %d\n",*freq_tono,*contador_tono);


}


//Enviar valor a puerto
void out_port_ay(z80_int puerto,z80_byte value)
{

	//Resetear detector de silencio
	silence_detection_counter=0;


	//printf ("Out port ay chip. Puerto: %d Valor: %d\n",puerto,value);
	if (puerto==65533) {
		//Ver si seleccion de chip turbosound o 3 canales AY

		int value_sin_mascara=value & 156; //10011100

		if (total_ay_chips>1 && value_sin_mascara==156) {

		/*the IC selection still the same
		TS is 111111 XX
		I suppose 255 value for first chip, 254 for second and 253 for third, right?

		11 to 1st ay, 10 to 2nd, 01 to 3rd and 00 to SID
		yes
		I made a change to pan the channels, but you can start with these.
		TBBlue now uses bit 6 and 5 to pan
		1LR111XX
		its compatible with original TS, because the tracas are selected by 111111XX, got it?
		Example:
		select 2nd AY with audio on Right side only
		10111110
		2nd AY, left side only 11011110
		3rd AY, both sides: 11111101
		*/


		//if (turbosound_enabled.v &&
		//   (value==255 || value==254 || value==253)
		//)
		//{

			int value_chip=value&3;

			//printf ("ay chip selection: %d\n",value);
			if (value_chip==3) ay_chip_selected=0;
			if (value_chip==2) ay_chip_selected=1;
			if (value_chip==1 && total_ay_chips>2) ay_chip_selected=2;
		}


		else {

			//seleccion de registro
			ay_3_8912_registro_sel[ay_chip_selected]=value & 15; //evitamos valores fuera de rango

		}
	}
	else if (puerto==49149) {
		//valor a registro
		ay_3_8912_registros[ay_chip_selected][ay_3_8912_registro_sel[ay_chip_selected]&15]=value;


	//imprimir registros
		/*int r;
		for (r=0;r<15;r++)
			printf ("R%d=%x ",r,ay_3_8912_registros[r]);
		printf ("\n");
*/

		//Nota sobre registro 7 mixer:
		//Bit 6 controla la direccion del registro de I/O - registro R14 - de puerto paralelo
		//como no emulamos puerto paralelo, no nos debe preocupar esto
		//registro R15 en este chip ay3-8912 no se usa para nada


		if (ay_3_8912_registro_sel[ay_chip_selected] ==0 || ay_3_8912_registro_sel[ay_chip_selected] == 1) {
			//Canal A
			//establece_frecuencia_tono(0,&freq_tono_A[ay_chip_selected],&contador_tono_A[ay_chip_selected]);
			establece_frecuencia_tono(0,&freq_tono_A[ay_chip_selected]);

		}

                if (ay_3_8912_registro_sel[ay_chip_selected] ==2 || ay_3_8912_registro_sel[ay_chip_selected] == 3) {
                        //Canal B
			//establece_frecuencia_tono(2,&freq_tono_B[ay_chip_selected],&contador_tono_B[ay_chip_selected]);
			establece_frecuencia_tono(2,&freq_tono_B[ay_chip_selected]);

                }


                if (ay_3_8912_registro_sel[ay_chip_selected] ==4 || ay_3_8912_registro_sel[ay_chip_selected] == 5) {
                        //Canal C
			//establece_frecuencia_tono(4,&freq_tono_C[ay_chip_selected],&contador_tono_C[ay_chip_selected]);
			establece_frecuencia_tono(4,&freq_tono_C[ay_chip_selected]);
                }

		if (ay_3_8912_registro_sel[ay_chip_selected] ==6) {
			//Frecuencia ruido
			int freq_temp=ay_3_8912_registros[ay_chip_selected][6] & 31;
	       		//printf ("Valor registros ruido : %d Hz\n",freq_temp);
			freq_temp=freq_temp*16;

			//controlamos divisiones por cero
			if (!freq_temp) freq_temp++;

                        freq_ruido[ay_chip_selected]=FRECUENCIA_NOISE/freq_temp;
			//printf ("Frecuencia ruido: %d Hz\n",freq_ruido);

		        //freq_ruido realmente tiene frecuencia*2... dice cada cuando se conmuta de signo
        		//freq_ruido=freq_ruido*TEMP_MULTIPLICADOR;
        		freq_ruido[ay_chip_selected]=freq_ruido[ay_chip_selected]*2;



			if (freq_ruido[ay_chip_selected]>FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
	                  //debug_printf (VERBOSE_DEBUG,"Frequency noise %d out of range",freq_ruido[ay_chip_selected]/2);
        	          freq_ruido[ay_chip_selected]=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
        		}


        //si la frecuencia del ruido es exactamente igual a la del sonido
        //alteramos un poco el valor
        if ( freq_ruido[ay_chip_selected]==FRECUENCIA_CONSTANTE_NORMAL_SONIDO) freq_ruido[ay_chip_selected]=FRECUENCIA_CONSTANTE_NORMAL_SONIDO-10;



			//printf ("Frecuencia ruido final: %d Hz\n",freq_ruido);


		}

		//Envelope
		//Esto se ejecuta aunque desactivemos el envelope por linea de comandos... pero da igual, al final no se escuchara el envelope
                if (ay_3_8912_registro_sel[ay_chip_selected] == 11 || ay_3_8912_registro_sel[ay_chip_selected] == 12) {
			//debug_printf (VERBOSE_DEBUG,"Register Frequency Envelope ay");
			//contador de 16 bits?
	        	int freq_temp=ay_3_8912_registros[ay_chip_selected][11]+256*(ay_3_8912_registros[ay_chip_selected][12] & 0xFF);
			//debug_printf (VERBOSE_DEBUG,"Register counter envelope: %d",freq_temp);
		        //freq_temp=freq_temp*256;
	       		//printf ("Valor frecuencia envelope : %d Hz\n",freq_temp);

			/* Envolvente: En teoria debe ir desde 0.1 Hz a 6 KHz

			SI X=6927

			Minimo valor(maxima frecuencia)=1.

			X/1= aprox 6000 Hz

			Maximo valor (menor frecuencia)=65536.  65536
			X/65536=0.09

			multiplicamos por 10

			X=69270

			*/


		        //controlamos divisiones por cero
			if (!freq_temp) freq_temp++;
		        freq_envelope[ay_chip_selected]=FRECUENCIA_ENVELOPE/freq_temp;

		        if (freq_envelope[ay_chip_selected]>FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
		                //debug_printf (VERBOSE_DEBUG,"Frequency envelope %d out of range",freq_envelope[ay_chip_selected]);
		                freq_envelope[ay_chip_selected]=FRECUENCIA_CONSTANTE_NORMAL_SONIDO;
		        }


        //si la frecuencia del envelope es exactamente igual a la del sonido
        //alteramos un poco el valor
	//esto sucede con valores demasiado altos y quedan fijados a FRECUENCIA_CONSTANTE_NORMAL_SONIDO en el trozo de codigo anterior


	//si lo alterasemos solo un poquito, sucede que juegos como el Robocop 2 no se oyen los disparos
	//lo alteramos mas ( *2 / 3)
        if ( freq_envelope[ay_chip_selected]==FRECUENCIA_CONSTANTE_NORMAL_SONIDO) {
		//printf ("temp freq_envelope = FRECUENCIA_CONSTANTE_NORMAL_SONIDO : %d\n",freq_envelope);
		freq_envelope[ay_chip_selected] *=2;
		freq_envelope[ay_chip_selected] /=3;
	}



		        //debug_printf (VERBOSE_DEBUG,"Frequency envelope *10 : %d Hz",freq_envelope[ay_chip_selected]);




                }

                //Envelope
		//Esto se ejecuta aunque desactivemos el envelope por linea de comandos... pero da igual, al final no se escuchara el envelope
                if (ay_3_8912_registro_sel[ay_chip_selected] == 13) {
			//debug_printf (VERBOSE_DEBUG,"Register Envelope Type ay : %d",value);
                        //Resetear el ciclo de envolvente
                        ciclo_envolvente[ay_chip_selected]=0;

		}




	}
}

//Activa chip ay en el caso que la opcion de autoactivado este habilitada y el chip este desactivado
void activa_ay_chip_si_conviene(void)
{
	if (ay_chip_present.v==0) {
		if (autoenable_ay_chip.v) {
			debug_printf (VERBOSE_INFO,"Autoenabling AY Chip");
			ay_chip_present.v=1;
		}
	}
}