#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

static const struct iio_chan_spec pmsa_channels[] = {
	{
		.type = IIO_CONCENTRATION,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
	},
}; 
struct pmsa_client {
   struct i2c_client *client;
   // May need a lock to handle simultanious attempts to use client
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

#define PMSA_NUM_REG 31

static int pmsa_read_raw(struct iio_dev *indio_dev,
      struct iio_chan_spec const *chan,int max_len,
      int *vals,int *val_len, long mask)
{
   struct pmsa_client *data = iio_priv(indio_dev);
   int status;
   
   char recv_buf[PMSA_NUM_REG] = {0};
   status = i2c_master_recv(data->client, recv_buf, PMSA_NUM_REG);
   if (status != PMSA_NUM_REG){
      pr_info("Failed to read data from pmsa");
   }
   
   vals[0] = (recv_buf[4] << 8) | (recv_buf[5]); // TODO: Replace these indexes with defines
   vals[1] = (recv_buf[6] << 8) | (recv_buf[7]); // TODO: Replace these indexes with defines
   vals[2] = (recv_buf[8] << 8) | (recv_buf[9]); // TODO: Replace these indexes with defines
   *val_len = 3;

   // TODO: Validate data with CRC
   
   return IIO_VAL_INT_MULTIPLE;
}

static const struct iio_info pmsa_info = {
	.read_raw_multi = pmsa_read_raw,
};

static int pmsa_probe(struct i2c_client *client, const struct i2c_device_id *){
   struct pmsa_client *data;
   int status;
   status = i2c_smbus_read_byte_data(client, 0x01);
   // TODO: Verify ID

   struct iio_dev *indio_dev;
   indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

   data = iio_priv(indio_dev);
   data->client = client;

   indio_dev->info = &pmsa_info;
   indio_dev->name = "pmsa"; // This should be derived from somehwere else
   indio_dev->modes = INDIO_DIRECT_MODE;
   indio_dev->channels = pmsa_channels;
   indio_dev->num_channels = ARRAY_SIZE(pmsa_channels);

	return devm_iio_device_register(&client->dev, indio_dev);
}

void pmsa_remove(struct i2c_client *client){
   // TODO: May need to free i2c or iio allocated mem
	pr_info("PMSA Remove");
}

static struct i2c_driver pmsa_driver = {
      .driver = {
              .name   = "pmsa003i",
              .of_match_table = of_match_ptr(pmsa_of_match),
      },
      .id_table       = pmsa003i_ids,
      .probe          = pmsa_probe,
      .remove         = pmsa_remove,
};

module_i2c_driver(pmsa_driver);

MODULE_LICENSE("GPL");
