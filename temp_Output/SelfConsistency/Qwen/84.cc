#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceFileClientServerCsma");

class UdpTraceFileClientServerCsma
{
public:
  static void PrintUsage()
  {
    std::cout << "Usage: ./waf --run=\"UdpTraceFileClientServerCsma [OPTIONS]\"\n";
    std::cout << "Options:\n";
    std::cout << "  --useIpv6              Enable IPv6 addressing (default: IPv4)\n";
    std::cout << "  --enableLogging        Enable simulation logging (default: disabled)\n";
    std::cout << "  --traceFile=FILENAME   Path to the trace file (default: \"src/applications/model/burst.trace\")\n";
  }

  int main(int argc, char *argv[])
  {
    bool useIpv6 = false;
    bool enableLogging = false;
    std::string traceFile = "src/applications/model/burst.trace";

    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 addresses", useIpv6);
    cmd.AddValue("enableLogging", "Enable logging", enableLogging);
    cmd.AddValue("traceFile", "Path to trace file", traceFile);
    cmd.Parse(argc, argv);

    if (enableLogging)
      {
        LogComponentEnable("UdpTraceFileClientServerCsma", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
        LogComponentEnable("UdpServer", LOG_LEVEL_ALL);
      }

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Creating CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    if (useIpv6)
      {
        stack.SetIpv4StackEnabled(false);
        stack.SetIpv6StackEnabled(true);
        stack.Install(nodes);
      }
    else
      {
        stack.Install(nodes);
      }

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
      {
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
        ipv6Interfaces.SetForwarding(0, true);
        ipv6Interfaces.SetDefaultRouteInAllNodes(0);
      }
    else
      {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
      }

    NS_LOG_INFO("Setting up UDP server and client.");
    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpTraceFileClientHelper client;
    if (useIpv6)
      {
        client = UdpTraceFileClientHelper(ipv6Interfaces.GetAddress(1, 1), port, traceFile);
      }
    else
      {
        client = UdpTraceFileClientHelper(ipv4Interfaces.GetAddress(1), port, traceFile);
      }

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
  }
};

int
main(int argc, char *argv[])
{
  UdpTraceFileClientServerCsma program;
  return program.main(argc, argv);
}