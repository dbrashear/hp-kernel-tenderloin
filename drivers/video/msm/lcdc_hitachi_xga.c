/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/i2c.h>
#ifdef CONFIG_PMIC8058_PWM
#include <linux/mfd/pmic8058.h>
#include <linux/pmic8058-pwm.h>
#endif
#include <mach/gpio.h>
#include "msm_fb.h"
#include <linux/platform_device.h>
#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#endif


#ifdef CONFIG_PMIC8058_PWM
static struct pwm_device *bl_pwm0;

/* for HITACHI panel 300hz was the minimum freq where flickering wasnt
 * observed as the screen was dimmed
 */

#define PWM_FREQ_HZ 300
#define PWM_PERIOD_USEC (USEC_PER_SEC / PWM_FREQ_HZ)
#define PWM_LEVEL 15
#define PWM_DUTY_LEVEL (PWM_PERIOD_USEC / PWM_LEVEL)
#endif

struct lcm_data {
	struct i2c_client *clientp;
};

static int tx18d42vm_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int __exit tx18d42vm_i2c_remove(struct i2c_client *client);

static const struct i2c_device_id tx18d42vm_i2c_id[] = {
	{"lcdxpanel", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tx18d42vm_i2c_id);

static struct i2c_driver tx18d42vm_i2c_driver = {
	.driver = {
		.name = "lcdxpanel",
		.owner = THIS_MODULE,
	},
	.probe      = tx18d42vm_i2c_probe,
 	.remove     = __exit_p(tx18d42vm_i2c_remove),
	.id_table   = tx18d42vm_i2c_id,

};

#ifdef CONFIG_SPI_QUP
static struct spi_device *lcdc_hitachi_spi_client;

static int __devinit lcdc_hitachi_spi_probe(struct spi_device *spi)
{
	lcdc_hitachi_spi_client = spi;
	lcdc_hitachi_spi_client->bits_per_word = 16;
	return 0;
}
static int __devexit lcdc_hitachi_spi_remove(struct spi_device *spi)
{
	lcdc_hitachi_spi_client = NULL;
	return 0;
}
static struct spi_driver lcdc_hitachi_spi_driver = {
	.driver = {
		.name  = "lcdc_hitachi_tx18d42vm",
		.owner = THIS_MODULE,
	},
	.probe         = lcdc_hitachi_spi_probe,
	.remove        = __devexit_p(lcdc_hitachi_spi_remove),
};

static int hitachi_spi_write(u8 reg, u8 data)
{
    u8 tx_buf[2];
    int ret;
    struct spi_message  m;
	struct spi_transfer t;
         
	if (!lcdc_hitachi_spi_client) {
		printk(KERN_ERR "%s lcdc_hitachi_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	spi_setup(lcdc_hitachi_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = reg;
	tx_buf[1] = data;
	t.rx_buf = NULL;
	t.len = 2;
	ret = spi_sync(lcdc_hitachi_spi_client, &m);
    return ret;
}

struct hitachi_spi_data {
	u8 addr;
	u8 data;
};

static struct hitachi_spi_data init_settings_ws1[] = {
	{  0x00<<2, 0x29 },  //RESET
	{  0x00<<2, 0x25 },  //STANBY
	{  0x02<<2, 0x40 },  //Enable Normally Black
	{  0x01<<2, 0x32 },  //Enable FRC/Dither
	{  0x03<<2, 0x06 },  //Gate On Sequence-Reverse"Z"
	{  0x0e<<2, 0x5F },  //Enter Test mode(1)
	{  0x0f<<2, 0xA4 },  //Enter Test mode(2)
	{  0x0d<<2, 0x09 },  //Enable SDRRS, enlarge OE width.
	//{  0x00<<2, 0xAD },  //DISPLAY ON
};

static struct hitachi_spi_data init_settings_ws2[] = {
	{  0x00<<2, 0x29 },  //RESET
	{  0x00<<2, 0x25 },  //STANBY
	{  0x02<<2, 0x40 },  //Enable Normally Black
	{  0x01<<2, 0x32 },  //Enable FRC/Dither
	{  0x0e<<2, 0x5F },  //Enter Test mode(1)
	{  0x0f<<2, 0xA4 },  //Enter Test mode(2)
	{  0x0d<<2, 0x09 },  //Enable SDRRS, enlarge OE width.
    {  0x10<<2, 0x41 },  //Adopt 2 Line / 1 Dot
};
#endif

enum hitachi_panel_version {
    WS1 = 0,
    WS2,
    EVT
};

unsigned int hitachi_panel_version = 0;
static struct msm_panel_common_pdata *lcdc_hitachi_pdata;

static int lcdc_hitachi_panel_on(struct platform_device *pdev)
{    
    int i=0;

    if(hitachi_panel_version == WS1)
    {
    #ifdef CONFIG_SPI_QUP
        for (i = 0; i < ARRAY_SIZE(init_settings_ws1); i++) 
        {
            hitachi_spi_write(init_settings_ws1[i].addr,
                           init_settings_ws1[i].data);
		if(i==0)
			mdelay(5);
        }
    #endif
    }
    else
    {
    #ifdef CONFIG_SPI_QUP
        for (i = 0; i < ARRAY_SIZE(init_settings_ws2); i++) 
        {
            hitachi_spi_write(init_settings_ws2[i].addr,
                           init_settings_ws2[i].data);
		if(i==0)
			mdelay(5);
        }
    #endif
    }

#ifdef CONFIG_SPI_QUP
    msleep(100);
    hitachi_spi_write(0x00<<2, 0xAD);   //DISPLAY ON
    
#endif

	return 0;
}

static int lcdc_hitachi_panel_off(struct platform_device *pdev)
{
	hitachi_spi_write(0x00<<2, 0xA5);   //STANBY-On
	return 0;
}


static void lcdc_hitachi_panel_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;
	int ret;

	bl_level = mfd->bl_level;

	if(lcdc_hitachi_pdata && lcdc_hitachi_pdata->pmic_backlight)
		lcdc_hitachi_pdata->pmic_backlight(bl_level);

#ifdef CONFIG_PMIC8058_PWM
	if (bl_pwm0) 
    {
		ret = pwm_config(bl_pwm0, PWM_DUTY_LEVEL * bl_level,
			PWM_PERIOD_USEC);
		if (ret)
			printk(KERN_ERR "pwm_config on pwm 0 failed %d\n", ret);
        
        ret = pwm_enable(bl_pwm0);
		if (ret)
			printk(KERN_ERR "pwm_enable on pwm 0 failed %d\n", ret);
	}
#endif

}

static int __init lcdc_hitachi_probe(struct platform_device *pdev)
{
    int ret=0;
    
	if (pdev->id == 0) {
		lcdc_hitachi_pdata = pdev->dev.platform_data;
		return 0;
	}

#ifdef CONFIG_PMIC8058_PWM
    if(lcdc_hitachi_pdata == NULL)
        return 0;
    
	bl_pwm0 = pwm_request(lcdc_hitachi_pdata->gpio_num[0], "backlight");
	if (bl_pwm0 == NULL || IS_ERR(bl_pwm0)) 
    {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_pwm0 = NULL;
	}

	printk(KERN_INFO "Lcdc_hitachi_probe: bl_pwm0=%p LPG_chan0=%d ",
			bl_pwm0, (int)lcdc_hitachi_pdata->gpio_num[0]
			);
#endif

	//msm_fb_add_device(pdev);
    	/* register EDID i2c interface driver */
	ret = i2c_add_driver(&tx18d42vm_i2c_driver);
    if (ret) 
    {
		printk(KERN_ERR "%s Failed to register tx18d42vm I2C driver error:%d\n",__func__, ret);     
	}
    
	return ret;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_hitachi_probe,
	.driver = {
		.name   = "lcdc_hitachi_xga",
	},
};

static struct msm_fb_panel_data hitachi_panel_data = {    
 	.on = lcdc_hitachi_panel_on,
 	.off = lcdc_hitachi_panel_off,   
	.set_backlight = lcdc_hitachi_panel_set_backlight,
};

static struct platform_device this_device = {
	.name   = "lcdc_hitachi_xga",
	.id	= 1,
	.dev	= {
		.platform_data = &hitachi_panel_data,
	}
};

struct tx18d42vm_register_s{
	char *name;
	u8	addr;
	u8	reg;
};

#define tx18d42vm_REGISTER(n, a) \
	{ .name = n, .addr = a }

#if 0
static struct tx18d42vm_register_s tx18d42vm_registers[] = {
	tx18d42vm_REGISTER("name1", 			0x08),
	tx18d42vm_REGISTER("name2", 			0x09),
	tx18d42vm_REGISTER("producet_code",	0x0a),
	tx18d42vm_REGISTER("hex", 			0x0b),
	tx18d42vm_REGISTER("sn1", 			0x0c),
	tx18d42vm_REGISTER("sn2", 			0x0d),
	tx18d42vm_REGISTER("sn3", 			0x0e),
	tx18d42vm_REGISTER("sn4", 			0x0f),
	tx18d42vm_REGISTER("week", 			0x10),
	tx18d42vm_REGISTER("year", 			0x11),
	tx18d42vm_REGISTER("edid_ver", 		0x12),
	tx18d42vm_REGISTER("edid_rever", 	0x13),

	tx18d42vm_REGISTER("pclk_lsb", 		0x36),
	tx18d42vm_REGISTER("pclk_msb",		0x37),
	tx18d42vm_REGISTER("hactive_l", 		0x38),
	tx18d42vm_REGISTER("hblanking_l", 	0x39),
	tx18d42vm_REGISTER("hactive_h",		0x3A),
	tx18d42vm_REGISTER("vactive", 		0x3B),
	tx18d42vm_REGISTER("vblanking", 		0x3C),
	tx18d42vm_REGISTER("vactive_h",		0x3D),
	tx18d42vm_REGISTER("hsync_off",		0x3E),
	tx18d42vm_REGISTER("hsync_pw",		0x3F),
	tx18d42vm_REGISTER("vsync_off_pw",	0x40),
	tx18d42vm_REGISTER("hvsync_off_w",	0x41),
	tx18d42vm_REGISTER("himage_size_mm",	0x42),
	tx18d42vm_REGISTER("vimage_size_mm",	0x43),
	tx18d42vm_REGISTER("hvimage_size",	0x44),
	tx18d42vm_REGISTER("hborder",		0x45),
	tx18d42vm_REGISTER("vborder",		0x46),
	tx18d42vm_REGISTER("hvsync_neg",		0x47),
	tx18d42vm_REGISTER("pclk_lsb2",		0x48),
	tx18d42vm_REGISTER("pclk_msb2",		0x49),
	tx18d42vm_REGISTER("hactive_l2",		0x4a),
	tx18d42vm_REGISTER("hblanking_l2",	0x4B),
	tx18d42vm_REGISTER("hactive_h2",		0x4C),
	tx18d42vm_REGISTER("vactive2",		0x4D),
	tx18d42vm_REGISTER("vblanking2",		0x4E),
	tx18d42vm_REGISTER("vactive_h2",		0x4F),
	tx18d42vm_REGISTER("hsync_off2",		0x50),
	tx18d42vm_REGISTER("hsync_pw2",		0x51),
	tx18d42vm_REGISTER("vsync_off_pw2",	0x52),
	tx18d42vm_REGISTER("hvsync_off_w2",	0x53),
	tx18d42vm_REGISTER("himage_size_mm2",0x54),
	tx18d42vm_REGISTER("vimage_size_mm2",0x55),
	tx18d42vm_REGISTER("hvimage_size2",	0x56),
	tx18d42vm_REGISTER("hborder2",		0x57),
	tx18d42vm_REGISTER("vborder2",		0x58),
	tx18d42vm_REGISTER("hvsync_neg2",	0x59),
	tx18d42vm_REGISTER("check_sum",		0x7F),
};

static int tx18d42vm_i2c_read(struct lcm_data *lcm_data)
{
	int i, rc;
	u8 data;
	struct lcm_data *ld = lcm_data;

	for(i = 0; i < ARRAY_SIZE(tx18d42vm_registers); i++)
	{
		struct i2c_msg msgs[] = {
			[0] = {
				.addr   = ld->clientp->addr,
				.flags  = 0,
				.buf    = (void *)&tx18d42vm_registers[i].addr,
				.len    = 1,
			},
			[1] = {
				.addr   = ld->clientp->addr,
				.flags  = I2C_M_RD,
				.buf    = (void *)&data,
				.len    = 1,
			}
		};

		rc = i2c_transfer(ld->clientp->adapter, msgs, 2);
		if (rc < 0)
		{
			pr_err("%s: i2c_transfer() failed(%d\n)", __func__, rc);
		}
		else
		{
			tx18d42vm_registers[i].reg = data;
			pr_info("%s: [%s][0x%02x]: 0x%02x\n", __func__, tx18d42vm_registers[i].name, tx18d42vm_registers[i].addr, tx18d42vm_registers[i].reg);
		}
	
	}

	return 0;
}
#endif

static int get_hitachi_panel_version(struct lcm_data *lcm_data)
{
    int rc;
	u8 data;
    u8 addr = 0x0B;
	struct lcm_data *ld = lcm_data;
    

    struct i2c_msg msgs[] = {
        [0] = {
                .addr   = ld->clientp->addr,
				.flags  = 0,
				.buf    = (void *)&addr,
				.len    = 1,
			},
			[1] = {
				.addr   = ld->clientp->addr,
				.flags  = I2C_M_RD,
				.buf    = (void *)&data,
				.len    = 1,
			}
		};

    rc = i2c_transfer(ld->clientp->adapter, msgs, 2);
    if (rc < 0)
    {
			pr_err("%s: i2c_transfer() failed(%d\n)", __func__, rc);
    }
    
    pr_info("%s: hitachi version data:(%d\n)", __func__, data);

    if(data == 0x00)
    {
        hitachi_panel_version = WS1;
    }
    else if(data == 0x01) 
    {
        hitachi_panel_version = WS2;
    }
    else if(data == 0x02)
    {
        hitachi_panel_version = EVT;
    }
    
    return rc;
    
}

static int tx18d42vm_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct lcm_data *ld;
	struct lcdc_platform_data *lcdc_pdata;
    struct msm_panel_info *pinfo;
	int ret=0;
    
	ld = (struct lcm_data *)kzalloc(sizeof(struct lcm_data), GFP_KERNEL);
	if (NULL == ld)
	{
		pr_err("%s: failed to alloc lcm_data!\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	ld->clientp = client;
	i2c_set_clientdata(client, ld);
	lcdc_pdata = client->dev.platform_data;
	
	if (lcdc_pdata && lcdc_pdata->lcdc_power_save)
		lcdc_pdata->lcdc_power_save(1);

#if 0
	ret = tx18d42vm_i2c_read(ld);
#endif

    ret = get_hitachi_panel_version(ld);
    if (ret < 0)
    {
			pr_err("%s: get verion info  failed(%d\n)", __func__, ret);
    }

	pinfo = &hitachi_panel_data.panel_info;
	pinfo->xres = 1024;
	pinfo->yres = 768;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 64000000; 
	pinfo->bl_max = 15;
	pinfo->bl_min = 1;

    pinfo->lcdc.h_back_porch = 90;
    pinfo->lcdc.h_front_porch = 151;
    pinfo->lcdc.h_pulse_width = 70;
    pinfo->lcdc.v_back_porch = 13;
    pinfo->lcdc.v_front_porch = 8;
    pinfo->lcdc.v_pulse_width = 10;
    pinfo->lcdc.border_clr = 0;
    pinfo->lcdc.underflow_clr = 0xff;
    pinfo->lcdc.hsync_skew = 0;

    msm_fb_add_device(&this_device);

error:
	return ret;
}

static int __exit tx18d42vm_i2c_remove(struct i2c_client *client)
{
	struct lcm_data *ld = i2c_get_clientdata(client);
    
	if(ld)
	{
		kfree(ld);
	}
	return 0;
}

static void __exit tx18d42vm_i2c_exit(void)
{
	i2c_del_driver(&tx18d42vm_i2c_driver);
}

static int __init lcdc_hitachi_panel_init(void)
{
	int ret;

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

#ifdef CONFIG_SPI_QUP 
    ret = spi_register_driver(&lcdc_hitachi_spi_driver);       
    if (ret) 
    {
        printk(KERN_ERR "%s not able to register spi\n", __func__);
        platform_driver_unregister(&this_driver);
        return ret;
    }
#endif

	ret = platform_device_register(&this_device);
	if (ret)
	{
		platform_driver_unregister(&this_driver);
        spi_unregister_driver(&lcdc_hitachi_spi_driver);
	}
	return ret;
}

module_init(lcdc_hitachi_panel_init);

MODULE_DESCRIPTION("tx18d42vm hitachi lcdc panel driver");
MODULE_AUTHOR("Effie Yu");
MODULE_LICENSE("GPL");
