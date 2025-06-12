#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceFileClientServer");

class UdpTraceFileClientHelper : public ApplicationHelper
{
public:
    UdpTraceFileClientHelper(Address address, uint16_t port)
    {
        m_factory.SetTypeId("ns3::UdpTraceFileClient");
        m_factory.Set("RemoteAddress", AddressValue(address));
        m_factory.Set("RemotePort", UintegerValue(port));
        m_factory.Set("MaxPacketSize", UintegerValue(1472)); // 1500 - 20 (IP) - 8 (UDP)
    }

    ApplicationContainer Install(Ptr<Node> node) const
    {
        return ApplicationHelper::InstallPriv(node);
    }
};

int main(int argc, char *argv[])
{
    bool enableLogging = false;
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging (true/false)", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 addressing (true/false)", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpTraceFileClientServer", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpTraceFileClient", LOG_LEVEL_ALL);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
    {
        stack.EnableIpv6();
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = address.Assign(devices);
    }
    else
    {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = address.Assign(devices);
    }

    uint16_t serverPort = 4000;

    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    AddressValue remoteAddress;
    if (useIpv6)
    {
        remoteAddress = AddressValue(Inet6SocketAddress(ipv6Interfaces.GetAddress(1, 1), serverPort));
    }
    else
    {
        remoteAddress = AddressValue(InetSocketAddress(ipv4Interfaces.GetAddress(1), serverPort));
    }

    UdpTraceFileClientHelper client(remoteAddress.Get(), serverPort);
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-trace-file-client-server.tr");
    csma.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}