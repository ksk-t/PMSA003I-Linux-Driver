#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define U16_FROM_BUF(buf, high)   (((buf)[(high)] << 8) | (buf)[(high) + 1])

#define PMSA_NUM_REG 32
#define ID_SIZE (2)
#define REG_ID_HIGH (0x00)
#define REG_PM1_HIGH (0x04)
#define REG_PM1_LOW (0x05)
#define REG_PM2P5_HIGH (0x06)
#define REG_PM2P5_LOW (0x07)
#define REG_PM10_HIGH (0x08)
#define REG_PM10_LOW (0x09)
#define REG_CRC_HIGH (0x1E)
#define REG_CRC_LOW (0x1F)

static const struct iio_chan_spec pmsa_channels[] = {
	{
		.type = IIO_CONCENTRATION,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
	},
}; 
struct pmsa_client {
   struct i2c_client *client;
   struct mutex lock;
};

static const struct i2c_device_id pmsa003i_ids[] = {
	{ "pmsa003i", 0, },
	{ /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, pmsa003i_ids);

static const struct of_device_id pmsa_of_match[] = {
	{
		.compatible = "pt,pmsa003i",
		.data = 0
	},
	{ /* LIST END */ },
};
MODULE_DEVICE_TABLE(of, pmsa_of_match);

static bool valid_crc(char *buf){
   uint16_t sum = 0;
   uint16_t expected_sum = U16_FROM_BUF(buf, REG_CRC_HIGH);
   const size_t CRC_SIZE = 2;
   for (size_t i = 0; i < PMSA_NUM_REG - CRC_SIZE; i++)
   {
      sum += buf[i]; 
   }
   return sum == expected_sum;
}

static int pmsa_read_raw(struct iio_dev *indio_dev,
      struct iio_chan_spec const *chan,int max_len,
      int *vals,int *val_len, long mask)
{
   struct pmsa_client *data = iio_priv(indio_dev);
   int status;
   
   char recv_buf[PMSA_NUM_REG] = {0};
   mutex_lock(&data->lock);
   status = i2c_master_recv(data->client, recv_buf, PMSA_NUM_REG);
   mutex_unlock(&data->lock);
   if (status != PMSA_NUM_REG){
      pr_info("Failed to read data from pmsa\n");
      *val_len = 0;
   }else{
      if (valid_crc((recv_buf)))
      {
         vals[0] = U16_FROM_BUF(recv_buf, REG_PM1_HIGH);
         vals[1] = U16_FROM_BUF(recv_buf, REG_PM2P5_HIGH);
         vals[2] = U16_FROM_BUF(recv_buf, REG_PM10_HIGH);
         *val_len = 3;
      }else {
         pr_info("CRC mismatch\n");
         *val_len = 0;
      }
   }
   
   return IIO_VAL_INT_MULTIPLE;
}

static const struct iio_info pmsa_info = {
	.read_raw_multi = pmsa_read_raw,
};

static int pmsa_probe(struct i2c_client *client, const struct i2c_device_id *){
   struct pmsa_client *data;
   int status;
   char recv_buf[PMSA_NUM_REG];
   status = i2c_master_recv(client, recv_buf, PMSA_NUM_REG);
   if (!valid_crc(recv_buf)){
      pr_info("CRC mismatch\n");
      return -ENODEV;
   }

   uint16_t expected_id = (0x42 << 8) | (0x4d);
   uint16_t actual_id = U16_FROM_BUF(recv_buf, REG_ID_HIGH);
   if (expected_id != actual_id){
      pr_info("ID:%d does not match expected value\n", actual_id);
      return -ENODEV;
   }

   struct iio_dev *indio_dev;
   indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

   data = iio_priv(indio_dev);
   data->client = client;
   mutex_init(&data->lock);
   
   indio_dev->info = &pmsa_info;
   indio_dev->name = client->name;
   indio_dev->modes = INDIO_DIRECT_MODE;
   indio_dev->channels = pmsa_channels;
   indio_dev->num_channels = ARRAY_SIZE(pmsa_channels);

	return devm_iio_device_register(&client->dev, indio_dev);
}

static struct i2c_driver pmsa_driver = {
      .driver = {
              .name   = "pmsa003i",
              .of_match_table = of_match_ptr(pmsa_of_match),
      },
      .id_table       = pmsa003i_ids,
      .probe          = pmsa_probe,
};

module_i2c_driver(pmsa_driver);

MODULE_LICENSE("GPL");
