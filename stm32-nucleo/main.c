#include <stdint.h>
#include <stdio.h>

// RCC
#define PERIPH_BASE       0x40000000UL
#define APB1PERIPH_BASE   (PERIPH_BASE + 0x00000000UL)
#define APB2PERIPH_BASE   (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE   (PERIPH_BASE + 0x00020000UL)

#define RCC_BASE          (AHB1PERIPH_BASE + 0x3800UL)

#define RCC_AHB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x30UL))
#define RCC_APB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x40UL))
#define RCC_APB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x44UL))

// GPIOA
#define GPIOA_BASE        (AHB1PERIPH_BASE + 0x0000UL)

#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_AFRL        (*(volatile uint32_t *)(GPIOA_BASE + 0x20UL))
#define GPIOA_AFRH        (*(volatile uint32_t *)(GPIOA_BASE + 0x24UL))

/*
    TIM2
    PA0 = PAN
    PA1 = TILT
*/
#define TIM2_BASE         (APB1PERIPH_BASE + 0x0000UL)

#define TIM2_CR1          (*(volatile uint32_t *)(TIM2_BASE + 0x00UL))
#define TIM2_EGR          (*(volatile uint32_t *)(TIM2_BASE + 0x14UL))
#define TIM2_CCMR1        (*(volatile uint32_t *)(TIM2_BASE + 0x18UL))
#define TIM2_CCER         (*(volatile uint32_t *)(TIM2_BASE + 0x20UL))

#define TIM2_PSC          (*(volatile uint32_t *)(TIM2_BASE + 0x28UL))
#define TIM2_ARR          (*(volatile uint32_t *)(TIM2_BASE + 0x2CUL))

#define TIM2_CCR1         (*(volatile uint32_t *)(TIM2_BASE + 0x34UL))
#define TIM2_CCR2         (*(volatile uint32_t *)(TIM2_BASE + 0x38UL))

/*
    USART1
    PA10 = RX
    NUCLEO D2
*/
#define USART1_BASE       (APB2PERIPH_BASE + 0x1000UL)

#define USART1_SR         (*(volatile uint32_t *)(USART1_BASE + 0x00UL))
#define USART1_DR         (*(volatile uint32_t *)(USART1_BASE + 0x04UL))
#define USART1_BRR        (*(volatile uint32_t *)(USART1_BASE + 0x08UL))
#define USART1_CR1        (*(volatile uint32_t *)(USART1_BASE + 0x0CUL))

// RCC bits
#define GPIOAEN           (1U << 0)
#define TIM2EN            (1U << 0)
#define USART1EN          (1U << 4)

// USART bits
#define USART_RXNE        (1U << 5)
#define USART_RE          (1U << 2)
#define USART_UE          (1U << 13)

// servo settings
#define PAN_BASE          90
#define TILT_BASE         90

#define SERVO_MIN_US      500
#define SERVO_MAX_US      2500

// angle to PWM
uint32_t angle_to_us(int32_t angle) {
    if (angle < 0) {angle = 0;}
    if (angle > 180) {angle = 180;}

    return SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180;
}

// servo functions
void servo_pan_set_angle(int32_t angle) {
    TIM2_CCR1 = angle_to_us(angle);
}

void servo_tilt_set_angle(int32_t angle) {
    TIM2_CCR2 = angle_to_us(angle);
}

// PWM initialization
void pwm_init() {

    RCC_AHB1ENR |= GPIOAEN;
    RCC_APB1ENR |= TIM2EN;

    // PA0 = Alternate Function
    GPIOA_MODER &= ~(3U << (0 * 2));
    GPIOA_MODER |=  (2U << (0 * 2));

    // PA1 = Alternate Function
    GPIOA_MODER &= ~(3U << (1 * 2));
    GPIOA_MODER |=  (2U << (1 * 2));

    // PA0 = AF1 = TIM2_CH1
    GPIOA_AFRL &= ~(0xFU << (0 * 4));
    GPIOA_AFRL |=  (1U << (0 * 4));

    // PA1 = AF1 = TIM2_CH2
    GPIOA_AFRL &= ~(0xFU << (1 * 4));
    GPIOA_AFRL |=  (1U << (1 * 4));

    /*
        Timer clock = 16 MHz
        Prescaler = 16
        16 MHz / 16 = 1 MHz
        1 timer tick = 1 us
    */
    TIM2_PSC = 16 - 1;

    // Servo PWM: 20000 us = 20 ms = 50 Hz
    TIM2_ARR = 20000 - 1;

    // CH1 PWM mode 1
    TIM2_CCMR1 &= ~(7U << 4);
    TIM2_CCMR1 |=  (6U << 4);

    // preload enable
    TIM2_CCMR1 |= (1U << 3);

    // CH2 PWM mode 1
    TIM2_CCMR1 &= ~(7U << 12);
    TIM2_CCMR1 |=  (6U << 12);

    // preload enable
    TIM2_CCMR1 |= (1U << 11);

    // Enable CH1
    TIM2_CCER |= (1U << 0);

    // Enable CH2
    TIM2_CCER |= (1U << 4);

    // Update registers
    TIM2_EGR |= 1U;

    // Start TIM2
    TIM2_CR1 |= 1U;
}

// UART initialization
void uart1_init() {

    RCC_AHB1ENR |= GPIOAEN;
    RCC_APB2ENR |= USART1EN;

    // PA10 = Alternate Function
    GPIOA_MODER &= ~(3U << (10 * 2));
    GPIOA_MODER |=  (2U << (10 * 2));

    // PA10 = AF7 = USART1_RX
    GPIOA_AFRH &= ~(0xFU << ((10 - 8) * 4));
    GPIOA_AFRH |=  (7U << ((10 - 8) * 4));

    /*
        USART clock = 16 MHz
        baud = 115200
    */
    USART1_BRR = 139U;

    // Enable receiver
    USART1_CR1 |= USART_RE;

    // Enable USART1
    USART1_CR1 |= USART_UE;
}

// check UART
int uart1_available() {
    return (USART1_SR & USART_RXNE) != 0;
}

// read one char
char uart1_read_char() {
    return (char)(USART1_DR & 0xFF);
}

int main(void) {

    pwm_init();
    uart1_init();

    // Initial servo position
    servo_pan_set_angle(PAN_BASE);
    servo_tilt_set_angle(TILT_BASE);

    // UART buffer
    char uart_buffer[32];
    uint32_t uart_index = 0;

    while (1) {

        // check if UART received a character
        if (uart1_available()) {

            char c = uart1_read_char();

            // end of UART command
            if (c == '\n') {

                // finish C string
                uart_buffer[uart_index] = '\0';

                int pan_angle;
                int tilt_angle;

                // get pan and tilt angles from "90,90"
                if (sscanf(uart_buffer, "%d,%d", &pan_angle, &tilt_angle) == 2) {

                    // check servo angle limits
                    if (
                        pan_angle >= 0 &&
                        pan_angle <= 180 &&
                        tilt_angle >= 0 &&
                        tilt_angle <= 180
                    ) {

                        // update servos
                        servo_pan_set_angle(pan_angle);
                        servo_tilt_set_angle(tilt_angle);
                    }
                }

                // clear buffer for next command
                uart_index = 0;
            }

            else {

                // add character to buffer
                if (uart_index < sizeof(uart_buffer) - 1) {
                    uart_buffer[uart_index] = c;
                    uart_index++;
                }

                else {

                    // buffer overflow: reset invalid packet
                    uart_index = 0;
                }
            }
        }
    }
}
