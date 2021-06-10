#include "stm32f4xx_hal.h"
#include "usart.h"
#include "gpio.h"
#include "stdio.h"
#include "md5.h"


/* �������� */ 
// ��ʼ�� 
void Program_Init(void);
void SystemClock_Config(void);
// ������ 
void Error_Reminder(void);
void Auth_Error_Reminder(void);
int Check_Led_order(uint8_t buffer[4]);
int Check_time_format(uint8_t buffer[4]);
int Char_to_int(uint8_t* buffer);
// �����֤
void Save_Host_Key(void);
void Save_User_Key(uint8_t buffer[8]);
int Verify_User(void);
int Check_hostkey_change(void);
int Check_userkey_change(void);
char* Decrypt(char* crypt);
char* Encrypt(char* text);
unsigned int Parity_Check_generate(char* tmp);
int Parity_check(char* tmp,unsigned int checksum);
// ����ִ��
void Play_Led(uint8_t* order, uint32_t time);
void Choose_LED_Turn(uint8_t i);
void Turn_On_LED(uint8_t LED_NUM, uint8_t buffer[4]);

//RNG
uint8_t RNG_Init(void);
void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng);
uint32_t RNG_Get_RandomNum(void);
uint32_t RNG_Get_RandomNumRange(int min, int max);
//us��ʱ
void Delay_us(uint32_t us);
//������Ⱥ�ʱ���delay
void addRandomDelay(int flag);
//buff��ʼ��
void buffInit();

//���ݽṹ��
struct backup{
	char* key;
	unsigned int checksum;
};


/* ȫ�ֱ��� */ 
struct backup user_backup1;
uint8_t Buff[8] = {0};// ��ʼ��
struct backup user_backup2;
int error_flag = 0;
RNG_HandleTypeDef RNG_Handler;//RNG���

// host key
#define HOST_KEY "\x25\xd5\x5a\xd2\x83\xaa\x40\x0a\xf4\x64\xc7\x6d\x71\x3c\x07\xad"
char host_key[16] = "\x25\xd5\x5a\xd2\x83\xaa\x40\x0a\xf4\x64\xc7\x6d\x71\x3c\x07\xad";
char *enc_host_key;
char *host_key_db;
unsigned int host_check;
unsigned int host_check_db;
// �û�����
char user_key[16];
char *enc_user_key;
//char *user_key_db;
unsigned int user_check;
//unsigned int user_check_db;


/* ��������� */ 

int main(void)
{
	//uint8_t mode = 0;
	uint8_t led_order[4] = {0};//LED˳��
	uint8_t time_arg[4] = {0};//ʱ��
	uint32_t time = 0;
	unsigned int init_time = 0;
	int flag = 0;
	unsigned int timeout = 0;
	uint32_t tickstart = 0;
	
	Program_Init();               // Ӳ����ʼ��
	buffInit();
	//RNG test
	/*RNG_Init();
	HAL_Delay(100);
	printf("RNG test: \r\n");
	for(int i = 0;i < 10;i++)
		printf("rand:%d, rand in range 1 to 50:%d\r\n", RNG_Get_RandomNum(),RNG_Get_RandomNumRange(1,2));*/	
	Save_Host_Key();              // ���ܱ���������Կ
  
	RNG_Init();
	addRandomDelay(RNG_Get_RandomNumRange(1,2));
	
	Loop1:
		error_flag = 0;
		tickstart = HAL_GetTick();  // ��ʱ��
		timeout = 0;
		if(init_time > 5)
			return 0;
		while(Buff[7] == 44)//δ����
		{
			addRandomDelay(RNG_Get_RandomNumRange(1,2));
			//����20s�ȴ�����
			//��ʱ�������򷵻�
			if(Buff[0] != 0){//���������
				MX_GPIO_Init();
				MX_USART1_UART_Init();
			}
			if(((HAL_GetTick() - tickstart) < 0) ||((HAL_GetTick() - tickstart) > 20000) || timeout > 0xFFFFFFF)
			{
				printf("���볬ʱ��\r\n");	
				Program_Init();
				Save_Host_Key();
				init_time++;
				goto Loop1; 
			}
			//�޳�ʱ��ͨ��
			if(error_flag == 0){
				HAL_UART_Receive_IT(&huart1, Buff, 8);
				//printf("ͨ��Buff[7]: %d\n", Buff[7]);
			}
			timeout++;			
		}
		
		error_flag = 1;
		//ȡģ��ֹ���
		/*printf("mod 8:\n");
		for(int i = 0; i < 8; i++){
			Buff[i] = Buff[i % 8];
			printf("%d", Buff[i]);
		}*/
		/* ����û������Ƿ�Ϸ� */
		for(int i = 0; i < 4; i++)
		{
			led_order[i] = Buff[i];
			time_arg[i] = Buff[4 + i];
		}
		addRandomDelay(RNG_Get_RandomNumRange(1,2));
		if(!Check_Led_order(led_order) || !Check_time_format(time_arg))//���led�Ƶ�˳���ʱ���Ƿ�Ϸ�
		{                           
			Error_Reminder();
			goto Loop1;
		}
		printf("�����ʽ��ȷ�\\n");
		Save_User_Key(Buff);        // ���ܱ����û���Կ
		time = Char_to_int(time_arg);              // ʱ�Ӳ����ַ���ת����
	
	/* У����Կ���û���� */
	flag = Check_userkey_change() && Check_hostkey_change();
	flag = flag ? Verify_User() : 0;
	srand(time);
	time = rand() % 500 + 500;
	addRandomDelay(RNG_Get_RandomNumRange(1,2));
	switch(flag)
	{
		case 0:                  // δͨ��
			Auth_Error_Reminder();
			//Play_Led(led_order, time);
			break;
		case 1:                  // ͨ��
			printf("��֤�ɹ�����ӭ��\n");
			Play_Led(led_order, time);
			for(int i = 0; i < 8; i++){
				Buff[i] = 0;
			}
			printf("\n");
			break;
		default:
			break;
	}
	
	goto Loop1;
}


/* ������ʵ�� */ 

//////////////////////////////��ʼ��//////////////////////////////////

// ����Ӳ����ʼ�� 
void Program_Init(void)
{	
	HAL_Init();
	// Configure the system clock 
	SystemClock_Config();	
	// Initialize all configured peripherals
	MX_GPIO_Init();
	MX_USART1_UART_Init();
	printf("123456789123456789123456789\n");
	printf("------------��֤ϵͳ----------------\n");
	printf("����������4λ��������Լ�4λ����ʱ��\n");
	printf("������ţ�1��2��3��4�ɸı�˳��\n");
	printf("����ʱ�Ӳ���������Ϊ4λ�����㲹0\n");
	printf("ʾ����������:12341234\n");	
	printf("------------------------------------\n");
}

// ϵͳʱ������ 
void SystemClock_Config(void)
{
 
  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

  __PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	
}

// �û��жϽ��ջص�����
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	HAL_UART_Transmit(&huart1, Buff, 8,100);
}

//  �����ض���printf����ַ�������
int fputc(int ch, FILE *f)
{ 
	uint8_t tmp[1]={0};
	tmp[0] = (uint8_t)ch;
	HAL_UART_Transmit(&huart1,tmp,1,10);	
	return ch;
}


//////////////////////////////������//////////////////////////////////

// ������ʾ:�������� 
void Error_Reminder(void)
{
	printf("\n �����ʽ����ȷ�����������룡\n");
	int n = 0;
	while(n < 200){
		HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_SET);  // �򿪷�����
		HAL_Delay(2);  // 50Hz
		HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_RESET);  // �رշ�����
		HAL_Delay(2);  // 50Hz
		n++;
	}		
	// Buff��ʼ�� 
	//int tmpLen = strlen(Buff);
	//printf("buffer len: %d\n", tmpLen);
	for(int i = 0; i < 8; i++){
		Buff[i] = 0;
	}
	Buff[7] = 44;
	//tmpLen = strlen(Buff);
	//printf("after change len: %d\n", tmpLen);
} 

void Auth_Error_Reminder(void)
{
	printf("\n��֤ʧ�ܣ����������룡\n");
	int n = 0;
	while(n < 200){
		HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_SET);  // �򿪷�����
		HAL_Delay(5);  // 50Hz
		HAL_GPIO_WritePin(GPIOG,GPIO_PIN_6,GPIO_PIN_RESET);  // �رշ�����
		HAL_Delay(2);  // 50Hz
		n++;
	}		
	// Buff��ʼ�� 
/*	int tmpLen = strlen(Buff);
	printf("buffer len: %d", tmpLen);
	for(int i = 0; i < tmpLen; i++){
			Buff[i] = Buff[i % 8];
			printf("%d", Buff[i]);
		}*/
	for(int i = 0; i < 8; i++){
		Buff[i] = 0;
	}
	Buff[7] = 44;
}



// �ж�LED������Ƿ�Ϸ�:ÿ���Ƴ����ҽ�����һ�� 
int Check_Led_order(uint8_t buffer[4])
{
	int a = 0;
	int b = 0;
	int c = 0;
	int d = 0;
	for(int i = 0; i < 4; i++){
		switch (buffer[i])
		{
			case '1':
				// printf("a:%d\n",a);
				a+=1;
				break;
			case '2':
				b+=1;
				break;
			case '3':
				c+=1;
				break;
			case 52:
				d+=1;
				break;	
			default:
				break;	
		}
	}
	// printf("a:%d,b:%d,c:%d,d:%d", a, b, c, d);
	if(a==1 && b==1 && c==1 && d==1) 
		return 1;
	else 
		return 0;	
} 

// ���ʱ���Ƿ�Ϸ�
int Check_time_format(uint8_t buffer[4])
{
    for(int i = 0; i < 4; i++){
    	if(buffer[i] < '0' || buffer[i] > '9')
    		return 0;
	}
	return 1;
} 

// �ַ���ת���� 
int Char_to_int(uint8_t* buffer)
{
	int n = 0;
	int length = sizeof(buffer)/sizeof(buffer[0]);
  for(int i = 0; i < length; i++){
    n = n * 10 + ( buffer[i] - '0');
	}
	return n;
}

// ��������Կ���ܡ�����У���롢����
void Save_Host_Key(void)
{
	enc_host_key = Encrypt(host_key);
	host_check =  Parity_Check_generate(enc_host_key);
	printf("\n host checksum:%d\n", host_check);
	// ����
	host_check_db = host_check;
	host_key_db = enc_host_key;
}

// ���û���Կ���ܡ�����У���롢����
void Save_User_Key(uint8_t buffer[8])
{
	MD5Digest(buffer, user_key);  // ���û�������md5����
	enc_user_key = Encrypt(user_key);
	user_check =  Parity_Check_generate(enc_user_key);
	printf("\n user checksum:%d\n", user_check);
	// ����
	//user_check_db = user_check;
	//user_key_db = enc_user_key;
	user_backup1.checksum = user_check;
	user_backup2.checksum = user_check;
	user_backup1.key = enc_user_key;
	user_backup2.key = enc_user_key;
}

// ��֤�û����
int Verify_User(void)
{
	// �Ƚ��������Ƿ�һ��
	for(int i = 0; i < 16; i++)
	{
		if(host_key[i] != user_key[i])
			return 0;
	}
	return 1;
}

// ���admin��Կ������������ʵ��
int Check_hostkey_change(void)
{
	if(!Parity_check(enc_host_key, host_check))
	{
		if(!Parity_check(host_key_db, host_check_db))
		{
			return 0;
		}
		else
		{
			*enc_host_key = *host_key_db;
			host_check = host_check_db;
		}
	}
	*host_key_db = *enc_host_key;
	host_check_db = host_check;
	return 1;
}

// ������û���Կ������������ʵ��
int Check_userkey_change(void)
{

	int tmp0 = Parity_check(enc_host_key, user_check);
	int tmp1 = Parity_check(user_backup1.key, user_backup1.checksum );
	int tmp2 = Parity_check(user_backup2.key, user_backup2.checksum);
	if(tmp0 + tmp1 + tmp2 >= 2){
		if(user_check == user_backup1.checksum){
			*user_backup2.key = *enc_user_key;
			user_backup2.checksum = user_check;
		}
		else if(user_backup1.checksum == user_backup2.checksum){
			*enc_user_key = *user_backup1.key;
			user_check = user_backup2.checksum;
		}
		else if(user_check == user_backup2.checksum){
			*user_backup1.key = *enc_user_key;
			user_backup1.checksum = user_check;
		}
		return 1;
	}
	else{
		return 0;
	}
}

// ����У����
unsigned int Parity_Check_generate(char* tmp){
	unsigned int checksum = 0;
	int length;
	length = strlen(tmp);
	for(int i = 0; i < length; i++)
	{
		checksum += tmp[i];
	}
	return checksum;
}

// �Ƚ�У�����Ƿ�һ��
int Parity_check(char* tmp,unsigned int checksum){
	unsigned int  target = 0;
	target = Parity_Check_generate(tmp);
	if(checksum == target)
		return 1;
	else
		return 0;
}

// ����
char* Encrypt(char* text)
{
	char* crypt;
	crypt = (char *)malloc(64);
	int i = 0;
	int count=strlen(text);
	for (i = 0; i < count; i++)
	{
		crypt[i] = text[i] + i + 5;
	}
	crypt[i] = '\0';
	return crypt;
}

// ����
char* Decrypt(char* crypt)
{
	char* text;
	text = (char *)malloc(64);
	int i = 0;
	int count = strlen(crypt);
	for (i = 0; i < count; i++)
	{
		text[i] = crypt[i] - i - 5;
	}
	text[i] = '\0';
	return text;
}


//////////////////////////////����ִ��//////////////////////////////////

// ���ݲ���ִ�ж�Ӧ�Ĺ���
void Play_Led(uint8_t* order, uint32_t time)
{
	int count = 0;
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOF,GPIO_PIN_10,GPIO_PIN_SET); 
	HAL_GPIO_WritePin(GPIOH,GPIO_PIN_15,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_SET);
	
	while(count < 4){
		Turn_On_LED(count % 4, order);
		count ++;
		HAL_Delay(time);
	}
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOF,GPIO_PIN_10,GPIO_PIN_SET); 
	HAL_GPIO_WritePin(GPIOH,GPIO_PIN_15,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_SET);
	
}

// ѡ��LED��
void Choose_LED_Turn(uint8_t i)
{
	switch(i)
	{
		case '1':
			HAL_GPIO_WritePin(GPIOF,GPIO_PIN_10,GPIO_PIN_RESET);
			break;
		case '2':
			HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0,GPIO_PIN_RESET);
			break;
		case '3':
			HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_RESET);
			break;
		case '4':
			HAL_GPIO_WritePin(GPIOH,GPIO_PIN_15,GPIO_PIN_RESET);
			break;
		default:
		    break;
	}
}

// ģʽ1��LED
void Turn_On_LED(uint8_t LED_NUM, uint8_t buffer[4])
{
	switch(LED_NUM)
	{
        case 0:
					Choose_LED_Turn(buffer[0]);
          break;
        case 1:
					Choose_LED_Turn(buffer[1]);
          break;
        case 2:
					Choose_LED_Turn(buffer[2]);
          break;
        case 3:
					Choose_LED_Turn(buffer[3]);
          break;          
        default:
          break;
	}

}


//��ʼ��RNG
uint8_t RNG_Init(void){
	uint16_t retry = 0;
	RNG_Handler.Instance = RNG;
	HAL_RNG_Init(&RNG_Handler);//��ʼ��
	while(__HAL_RNG_GET_FLAG(&RNG_Handler, RNG_FLAG_DRDY) == RESET && retry < 10000)//�ȴ�RNG��������
	{
		retry++;
		//delay_us(10);
		HAL_Delay(1);
	}
	if(retry >= 10000)
		return 0;//RNG����������
	return 1;//RNG��������
}

//ʹ��RNGʱ��
void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng){
	__HAL_RCC_RNG_CLK_ENABLE();
}

uint32_t RNG_Get_RandomNum(void){
	return HAL_RNG_GetRandomNumber(&RNG_Handler);
}

//[min, max]�����
uint32_t RNG_Get_RandomNumRange(int min, int max){
	return HAL_RNG_GetRandomNumber(&RNG_Handler)%(max-min+1)+min;
}

//us delay
void Delay_us(uint32_t us){
	uint32_t currentTick = SysTick->VAL;
	const uint32_t tickPerMs = SysTick->LOAD + 1;//ÿms��tick��
	const uint32_t TickCounter = ((us - ((us > 0) ? 1 : 0)) * tickPerMs) / 1000;
	uint32_t elapsedTick = 0; //��ȥ��tick
	uint32_t oldTick = currentTick;
	do{
		currentTick = SysTick->VAL;
		elapsedTick += (oldTick < currentTick) ? tickPerMs + oldTick - currentTick : oldTick - currentTick;
		oldTick = currentTick;
	}while(TickCounter > elapsedTick);
}

//������Ⱥ�ʱ����delay
void addRandomDelay(int flag){
	RNG_Init();
	if(flag == 1)//flag = 1ʱus delay
		Delay_us(RNG_Get_RandomNumRange(1, 100));
	else//flag = 2ʱ ms_delay
		HAL_Delay(RNG_Get_RandomNumRange(1, 100));
}

void buffInit(){
	for(int i = 0; i < 8; i++){
		Buff[i] = 0;
	}
	Buff[7] = 44;
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
