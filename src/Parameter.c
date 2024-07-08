#include <pubsub/Parameter.h>

//#include <cstdlib>
#include <stdio.h>

#include <pubsub/Node.h>
#include <pubsub/Publisher.h>
#include <pubsub/Subscriber.h>
#include <pubsub/System.h>

//#include <pubsub/Parameters.msg.h>

#include <math.h>

static double param_internal_callback(const char* name, double value, void* data)
{
	struct ps_parameters* params = (struct ps_parameters*)data;
	for (int i = 0; i < params->msg.name_length; i++)
	{
		if (strcmp(name, params->msg.name[i]) == 0)
		{
			// enforce min/max
			if (value < params->msg.min[i])
			{
				value = params->msg.min[i];
			}
			else if (value > params->msg.max[i])
			{
				value = params->msg.max[i];
			}
			free(params->msg.value[i]);
			char valstr[50];
  			sprintf(valstr, "%lf", value);
  			params->msg.value[i] = strdup(valstr);
			params->callback(name, value, params->cb_data);
			ps_pub_publish_ez(&params->param_pub, &params->msg);
			return value;
		}
	}
	printf("could not find parameter %s\n", name);

	return nan("");
}

void ps_create_parameters(struct ps_node_t* node, struct ps_parameters* params_out, ps_param_fancy_cb_t callback, void* data)
{
  node->param_cb = param_internal_callback;
  node->param_cb_data = (void*)params_out;
  params_out->callback = callback;
  params_out->cb_data = data;
  params_out->msg.name_length = 0;
  ps_node_create_publisher(node, "/parameters", &pubsub__Parameters_def, &params_out->param_pub, true);
}

void ps_destroy_parameters(struct ps_parameters* params)
{
  params->param_pub.node->param_cb = 0;
  if (params->msg.name_length > 0)
  {
	// free everything!
	for (int i = 0; i < params->msg.name_length; i++)
	{
      free(params->msg.name[i]);
      free(params->msg.value[i]);
      free(params->msg.description[i]);
	}
	free(params->msg.name);
    free(params->msg.value);
    free(params->msg.description);
    free(params->msg.type);
    free(params->msg.min);
    free(params->msg.max);
  }
  ps_pub_destroy(&params->param_pub);
}
/*  { FT_String, FF_NONE, "name", 0, 0 }, 
  { FT_String, FF_NONE, "description", 0, 0 }, 
  { FT_UInt8, FF_ENUM, "type", 0, 0 }, 
  { FT_String, FF_NONE, "value", 0, 0 }, 
  { FT_Float64, FF_NONE, "min", 0, 0 }, 
  { FT_Float64, FF_NONE, "max", 0, 0 }, */
void ps_add_parameter_double(struct ps_parameters* params,
 const char* name, const char* description,
 double value, double min, double max)
{
  int old_len = params->msg.name_length;
  int new_len = ++params->msg.name_length;
  params->msg.name_length = params->msg.value_length = params->msg.min_length = 
  params->msg.max_length = params->msg.description_length = params->msg.type_length = new_len;
  char** newname = (char**)malloc(sizeof(char*)*new_len);
  char** newdesc = (char**)malloc(sizeof(char*)*new_len);
  char** newvalue = (char**)malloc(sizeof(char*)*new_len);
  double* newmin = (double*)malloc(sizeof(double)*new_len);
  double* newmax = (double*)malloc(sizeof(double)*new_len);
  uint8_t* newtype = (uint8_t*)malloc(sizeof(uint8_t)*new_len);
  for (int i = 0; i < old_len; i++)
  {
    newname[i] = params->msg.name[i];
    newdesc[i] = params->msg.description[i];
    newvalue[i] = params->msg.value[i];
    newtype[i] = params->msg.type[i];
    newmin[i] = params->msg.min[i];
    newmax[i] = params->msg.max[i];
  }
  newname[old_len] = strdup(name);
  newdesc[old_len] = strdup(description);
  char valstr[50];
  sprintf(valstr, "%lf", value);
  newvalue[old_len] = strdup(valstr);
  newtype[old_len] = PARAMETERS_DOUBLE;
  newmin[old_len] = min;
  newmax[old_len] = max;
  if (old_len != 0)
  {
	free(params->msg.name);
    free(params->msg.value);
    free(params->msg.description);
    free(params->msg.type);
    free(params->msg.min);
    free(params->msg.max);
  }
  params->msg.name = newname;
  params->msg.description = newdesc;
  params->msg.value = newvalue;
  params->msg.type = newtype;
  params->msg.min = newmin;
  params->msg.max = newmax;
  ps_pub_publish_ez(&params->param_pub, &params->msg);
}
