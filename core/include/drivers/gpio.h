/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2016, Linaro Limited
 */

#ifndef __GPIO_H__
#define __GPIO_H__

#include <dt-bindings/gpio/gpio.h>
#include <kernel/dt.h>
#include <kernel/dt_driver.h>
#include <stdint.h>
#include <tee_api_types.h>

/**
 * GPIO_DT_DECLARE - Declare a gpio controller driver with a single
 * device tree compatible string.
 *
 * @__name: Reset controller driver name
 * @__compat: Compatible string
 * @__probe: GPIO controller probe function
 */
#define GPIO_DT_DECLARE(__name, __compat, __probe) \
	static const struct dt_device_match __name ## _match_table[] = { \
		{ .compatible = __compat }, \
		{ } \
	}; \
	DEFINE_DT_DRIVER(__name ## _dt_driver) = { \
		.name = # __name, \
		.type = DT_DRIVER_GPIO, \
		.match_table = __name ## _match_table, \
		.probe = __probe, \
	}

enum gpio_dir {
	GPIO_DIR_OUT,
	GPIO_DIR_IN
};

enum gpio_level {
	GPIO_LEVEL_LOW,
	GPIO_LEVEL_HIGH
};

enum gpio_interrupt {
	GPIO_INTERRUPT_DISABLE,
	GPIO_INTERRUPT_ENABLE
};

struct gpio_chip {
	const struct gpio_ops *ops;
};

struct gpio_ops {
	enum gpio_dir (*get_direction)(struct gpio_chip *chip,
				       unsigned int gpio_pin);
	void (*set_direction)(struct gpio_chip *chip, unsigned int gpio_pin,
			      enum gpio_dir direction);
	enum gpio_level (*get_value)(struct gpio_chip *chip,
				     unsigned int gpio_pin);
	void (*set_value)(struct gpio_chip *chip, unsigned int gpio_pin,
			  enum gpio_level value);
	enum gpio_interrupt (*get_interrupt)(struct gpio_chip *chip,
					     unsigned int gpio_pin);
	void (*set_interrupt)(struct gpio_chip *chip, unsigned int gpio_pin,
			      enum gpio_interrupt ena_dis);
};

struct gpio {
	struct gpio_chip *chip;
	uint32_t dt_flags;
	unsigned int pin;
};

static inline bool gpio_ops_is_valid(const struct gpio_ops *ops)
{
	return ops->set_direction && ops->get_direction && ops->get_value &&
	       ops->set_value;
}

static inline void gpio_set_direction(struct gpio *gpio, enum gpio_dir dir)
{
	gpio->chip->ops->set_direction(gpio->chip, gpio->pin, dir);
}

static inline enum gpio_dir gpio_get_direction(struct gpio *gpio)
{
	return gpio->chip->ops->get_direction(gpio->chip, gpio->pin);
}

static inline void gpio_set_value(struct gpio *gpio, enum gpio_level value)
{
	if (gpio->dt_flags & GPIO_ACTIVE_LOW)
		value = !value;

	gpio->chip->ops->set_value(gpio->chip, gpio->pin, value);
}

static inline enum gpio_level gpio_get_value(struct gpio *gpio)
{
	enum gpio_level value = GPIO_LEVEL_LOW;

	value = gpio->chip->ops->get_value(gpio->chip, gpio->pin);

	if (gpio->dt_flags & GPIO_ACTIVE_LOW)
		value = !value;

	return value;
}

#if defined(CFG_DT) && defined(CFG_DRIVERS_GPIO)

struct gpio *gpio_get_dt_two_cells(struct dt_driver_phandle_args *a,
				   TEE_Result *res);

/**
 * gpio_dt_get_by_index - Get a gpio controller at a specific index in
 * 'gpios' property
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the subnode containing a 'gpios' property
 * @index: Reset controller index in '*-gpios' property
 * @gpio: Output gpio pin reference upon success
 *
 * Return TEE_SUCCESS in case of success
 * Return TEE_ERROR_DEFER_DRIVER_INIT if gpio controller is not initialized
 * Return a TEE_Result compliant code in case of error
 */
TEE_Result gpio_dt_get_by_index(const void *fdt, int nodeoffset,
				unsigned int index, const char *gpio_name,
				struct gpio **gpio);
#else
static inline
TEE_Result gpio_dt_get_by_index(const void *fdt __unused,
				int nodeoffset __unused,
				unsigned int index  __unused,
				const char *gpio_name  __unused,
				struct gpio **gpio)
{
	*gpio = NULL;
	return TEE_ERROR_NOT_SUPPORTED;
}

static inline
struct gpio *gpio_get_dt_two_cells(struct dt_driver_phandle_args *a __unused,
				   TEE_Result *res)
{
	*res = TEE_ERROR_NOT_SUPPORTED;

	return NULL;
}

#endif /*CFG_DT*/

/**
 * gpio_dt_get_func - Typedef of function to get gpio controller from
 * devicetree properties
 *
 * @a: Pointer to devicetree description of the gpio controller to parse
 * @data: Pointer to the data given at gpio_dt_register_provider() call
 * @res: Output result code of the operation:
 *	TEE_SUCCESS in case of success
 *	TEE_ERROR_DEFER_DRIVER_INIT if gpio controller is not initialized
 *	Any TEE_Result compliant code in case of error.
 *
 * Returns a struct gpio pointer pointing to a gpio controller matching
 * the devicetree description or NULL if invalid description in which case
 * @res provides the error code.
 */
typedef struct gpio *(*gpio_dt_get_func)(struct dt_driver_phandle_args *a,
					 void *data, TEE_Result *res);

/**
 * gpio_dt_register_provider - Register a gpio controller provider
 *
 * @fdt: Device tree to work on
 * @nodeoffset: Node offset of the gpio controller
 * @get_dt_gpio: Callback to match the gpio controller with a struct gpio
 * @gpio_chip: gpio_chip which will be passed to the get_dt_gpio callback
 * Returns TEE_Result value
 */
static inline
TEE_Result gpio_register_provider(const void *fdt, int nodeoffset,
				  gpio_dt_get_func get_dt_gpio, void *data)
{
	return dt_driver_register_provider(fdt, nodeoffset,
					   (get_of_device_func)get_dt_gpio,
					   data, DT_DRIVER_GPIO);
}

#endif	/* __GPIO_H__ */
