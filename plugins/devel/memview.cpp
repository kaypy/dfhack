#include "Core.h"
#include "Console.h"
#include "PluginManager.h"
#include "MemAccess.h"
#include "MiscUtils.h"
#include <../depends/tthread/tinythread.h> //not sure if correct
#include <string>
#include <vector>
#include <sstream>

using std::vector;
using std::string;
using namespace DFHack;

uint64_t timeLast=0;
static tthread::mutex* mymutex=0;

struct memory_data
{
	void * addr;
	size_t len;
	size_t refresh;
	int state;
	uint8_t *buf,*lbuf;
	vector<t_memrange> ranges;
}memdata;
enum HEXVIEW_STATES
{
	STATE_OFF,STATE_ON
};
DFhackCExport command_result memview (Core * c, vector <string> & parameters);

DFhackCExport const char * plugin_name ( void )
{
    return "memview";
}

DFhackCExport command_result plugin_init ( Core * c, std::vector <PluginCommand> &commands)
{
    commands.clear();
	commands.push_back(PluginCommand("memview","Shows memory in real time. Params: adrr length refresh_rate. If addr==0 then stop viewing",memview));
	memdata.state=STATE_OFF;
	mymutex=new tthread::mutex;
    return CR_OK;
}
size_t convert(const std::string& p,bool ishex=false)
{
	size_t ret;
	std::stringstream conv;
	if(ishex)
		conv<<std::hex;
	conv<<p;
	conv>>ret;
	return ret;
}
bool isAddr(uint32_t *trg,vector<t_memrange> & ranges)
{
	if(trg[0]%4==0)
		for(size_t i=0;i<ranges.size();i++)
			if(ranges[i].isInRange((void *)trg[0]))
				return true;

	return false;
}
void outputHex(uint8_t *buf,uint8_t *lbuf,size_t len,size_t start,Core *c,vector<t_memrange> & ranges)
{
	Console &con=c->con;
    const size_t page_size=16;
	con.clear();

	for(size_t i=0;i<len;i+=page_size)
	{
	    con.gotoxy(1,i/page_size+1);
		con.print("0x%08X ",i+start);
		for(size_t j=0;(j<page_size) && (i+j<len);j++)
			{
				if(j%4==0)
				{
					con.reset_color();

					if(isAddr((uint32_t *)(buf+j+i),ranges))
						con.color(Console::COLOR_LIGHTRED); //coloring in the middle does not work
                    //TODO make something better?
				}
				if(lbuf[j+i]!=buf[j+i])
					con.print("*%02X",buf[j+i]); //if modfied show a star
				else
					con.print(" %02X",buf[j+i]);
			}
		con.reset_color();
		con.print(" | ");
		for(size_t j=0;(j<page_size) && (i+j<len);j++)
			if((buf[j+i]>31)&&(buf[j+i]<128)) //only printable ascii
				con.print("%c",buf[j+i]);
			else
				con.print(".");
		//con.print("\n");
	}
	con.print("\n");
	con.flush();

}
void Deinit()
{
	if(memdata.state==STATE_ON)
	{
		memdata.state=STATE_OFF;
		delete [] memdata.buf;
		delete [] memdata.lbuf;
	}
}
DFhackCExport command_result plugin_onupdate ( Core * c )
{

	mymutex->lock();
	if(memdata.state==STATE_OFF)
	{
		mymutex->unlock();
		return CR_OK;
	}
	//Console &con=c->con;
	uint64_t time2 = GetTimeMs64();
	uint64_t delta = time2-timeLast;

	if(memdata.refresh!=0)
	if(delta<memdata.refresh)
	{
		mymutex->unlock();
		return CR_OK;
	}
	timeLast = time2;

	c->p->read(memdata.addr,memdata.len,memdata.buf);
	outputHex(memdata.buf,memdata.lbuf,memdata.len,(size_t)memdata.addr,c,memdata.ranges);
    memcpy(memdata.lbuf, memdata.buf, memdata.len);
	if(memdata.refresh==0)
		Deinit();
	mymutex->unlock();
	return CR_OK;

}
DFhackCExport command_result memview (Core * c, vector <string> & parameters)
{
	mymutex->lock();
	c->p->getMemRanges(memdata.ranges);
	memdata.addr=(void *)convert(parameters[0],true);
	if(memdata.addr==0)
	{
		Deinit();
		memdata.state=STATE_OFF;
		mymutex->unlock();
		return CR_OK;
	}
	else
	{
		Deinit();
		bool isValid=false;
		for(size_t i=0;i<memdata.ranges.size();i++)
			if(memdata.ranges[i].isInRange(memdata.addr))
				isValid=true;
		if(!isValid)
		{
			c->con.printerr("Invalid address:%x\n",memdata.addr);
			mymutex->unlock();
			return CR_OK;
		}
		memdata.state=STATE_ON;
	}
	if(parameters.size()>1)
		memdata.len=convert(parameters[1]);
	else
		memdata.len=20*16;

	if(parameters.size()>2)
		memdata.refresh=convert(parameters[2]);
	else
		memdata.refresh=0;


	uint8_t *buf,*lbuf;
	memdata.buf=new uint8_t[memdata.len];
	memdata.lbuf=new uint8_t[memdata.len];
	c->p->getMemRanges(memdata.ranges);
	mymutex->unlock();
	return CR_OK;
}
DFhackCExport command_result plugin_shutdown ( Core * c )
{
	mymutex->lock();
	Deinit();
	delete mymutex;
	mymutex->unlock();
	return CR_OK;
}
