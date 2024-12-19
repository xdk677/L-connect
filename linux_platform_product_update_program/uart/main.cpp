#include "SerialCommunication.h"
#include "mylog.h"
int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		std::cout << "ERROR:参数个数有误,第一个参数为串口文件名,第二个参数为待更新固件" << std::endl;
		return 0;
	}
	Log log("m2_update.log", 5 * 1024 * 1024, 1);
	//调用时候修改串口文件名，
	//SerialCommunication serialComm("/dev/ttyUSB0",std::ref(log));
	//serialComm.updateM2Firmware("./M2_firmware.dat");
	SerialCommunication serialComm(argv[1],std::ref(log));
	/*
	for(int i = 0; i < 1000; i ++)
	{
		log.write("*************************第%d次M2固件升级测试开始*************************",i+1);
		serialComm.updateM2Firmware(argv[2]);
		log.write("*************************第%d次M2固件升级测试完成*************************",i+1);
	}
	*/
	serialComm.updateM2Firmware(argv[2]);
	return 0;
}



