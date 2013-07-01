/**********************************************************************
** LPC Main 
** Designed By Luolong, @15.06,2009
** Copyright (c) LuoLong
** All Rights Reserved!
**********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "evmdm6437.h"
#include "lpc.h"


typedef struct
{
	short BufferIn1[BUFLEN];
	unsigned char BufferOut1[12];
}
LPC_STATE1;

LPC_STATE1 lpcstate1;


typedef struct
{
	unsigned char BufferIn2[12];
	short BufferOut2[LPC_SAMPLES_PER_FRAME];
}
LPC_STATE2;
	
LPC_STATE2 lpcstate2;
	 
lpc_encoder_state *state1;
lpc_decoder_state *state2;


//main 
void main()
{		
	FILE *fp1, *fp2, *fp3, *fp4, *fp5, *fp6;

	long LEN; //�ļ�����
	int i = 0;
	long length=0;

	unsigned char ss;
	unsigned char temp1;
	unsigned char temp2;
	short temp;

	short *in1;
	unsigned char *out1;

    unsigned char *in2;
	short *out2;

	EVMDM6437_init(); 

	in1 = lpcstate1.BufferIn1;
	out1 = lpcstate1.BufferOut1;

	in2 = lpcstate2.BufferIn2;
	out2 = lpcstate2.BufferOut2;

	init_lpc_encoder_state(state1);
	init_lpc_decoder_state(state2);
	
	fp1 = fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\encode_txt.txt","w");//���ı��ļ���д��ѹ���������,�ı���ʽ
	if (fp1 == NULL)
	{
		printf("�޷��򿪱����ı��ļ�!\n");
		exit(0);
	}

	fp2 = fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\encode_bit.txt","wb");//���ı��ļ���д��ѹ���������.��������ʽ
	if (fp2 == NULL)
	{
		printf("�޷��򿪱���������ļ�!\n");
		exit(0);
	}

	fp3 = fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\lpc.pcm","rb"); //��ԭʼ�����ļ� ,�����Ʒ�ʽ 
	if (fp3 == NULL)   
	{   
		printf("�޷���PCM�����ļ�!\n");    
		exit(0);   
	}
	else   
	{
		printf("*****�����Ǳ���*****\n");
	}

	
	fseek(fp3, 0L, SEEK_END);
	LEN = ftell(fp3);
	fseek(fp3, 0L, SEEK_SET);

	printf("��PCM�����ļ�ȡ������!\n");

    while (length <= LEN)
    {
    	for (i=0; i < BUFLEN; i++)
		{
			temp=0x0000;
			temp1=fgetc(fp3);//��λ
			temp2=fgetc(fp3);//��λ
			temp|=temp2;
			temp=(temp<<8);
			temp|=temp1;
			length+=2;
			in1[i]=temp;
			printf("%d\n",in1[i]);
		}

		lpc_encode(in1,out1,state1);

		for (i=0; i<12; i++)
		{
			fprintf(fp1,"%u",out1[i]);//д���ı��ļ�
			fprintf(fp1," ");
			fwrite(&out1[i],1,1,fp2);//д��������ļ�
		}
		fprintf(fp1,"\n");
	}

	fclose(fp1);
	fclose(fp2);
	fclose(fp3);
	
	fp4 = fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\encode_txt.txt","r");//�򿪱����ļ�,���ı�����
	if (fp4 == NULL)
	{
		printf("�޷��򿪱����ı��ļ�!\n");
		exit(0);
	}
	
	fp5 = fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\encode_bit.txt","rb");//�򿪱����ļ�,�������ƶ���
	if(fp5==NULL)
	{
		printf("�޷��򿪱���������ļ�!\n");
		exit(0);
	}
	
	fp6=fopen("c:\\CCStudio_v3.3\\MyProjects\\lpc9\\result_voice.pcm","wb");//�����ѹ�������
	if (fp6 == NULL)
	{ 
		printf("�޷��򿪽��������ļ�!\n"); 
		exit(0); 
	} 
	else
	{
		printf("*****�����ǽ���*****\n");
		while (1)
		{
			for (i=0; i<12; i++)//�������ƶ���ѹ���������
			{
				fread(&in2[i],1,1,fp5);
			}

			if (feof(fp5)) 
			{
				break;
			}
		
			lpc_decode(in2,out2,state2);

			for (i=0; i<LPC_SAMPLES_PER_FRAME; i++)
			{
				ss=((unsigned char)((unsigned short)out2[i]&0x00ff));
				fwrite(&ss,1,1,fp6);
				ss=((unsigned char)((unsigned short)(out2[i])>>8));
				fwrite(&ss,1,1,fp6);
			}			
		}		
	}

	_wait(500000);
	printf("�������!\n");
	SW_BREAKPOINT;
	
	return;
}
