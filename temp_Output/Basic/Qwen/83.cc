#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpExample");

class UdpFlowHelper {
public:
    static void Setup(Ptr<Node> sender, Ptr<Node> receiver, Ipv4Address ipv4Receiver, Ipv6Address ipv6Receiver,
                      bool useIpv6, bool enableLogging) {
        uint16_t port = 9;
        if (useIpv6) {
            AddressValue remoteAddress(Inet6SocketAddress(ipv6Receiver, port));
            UdpClientHelper client(remoteAddress);
            client.SetAttribute("PacketSize", UintegerValue(1024));
            client.SetAttribute("MaxPackets", UintegerValue(320));
            client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
            ApplicationContainer app = client.Install(sender);
            app.Start(Seconds(2.0));
            app.Stop(Seconds(10.0));

            PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv6Address::GetAny(), port));
            sink.Install(receiver);
        } else {
            AddressValue remoteAddress(InetSocketAddress(ipv4Receiver, port));
            UdpClientHelper client(remoteAddress);
            client.SetAttribute("PacketSize", UintegerValue(1024));
            client.SetAttribute("MaxPackets", UintegerValue(320));
            client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
            ApplicationContainer app = client.Install(sender);
            app.Start(Seconds(2.0));
            app.Stop(Seconds(10.0));

            PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            sink.Install(receiver);
        }

        if (enableLogging) {
            LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
            LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
        }
    }
};

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.AddValue("enableLogging", "Enable logging for applications", enableLogging);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    Config::SetDefault("ns3::CsmaChannel::DataRate", DataRateValue(DataRate("100Mbps")));
    Config::SetDefault("ns3::CsmaChannel::Delay", TimeValue(MicroSeconds(6560)));

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.SetIpv4StackInstall(true);
    stack.SetIpv6StackInstall(useIpv6);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv6AddressHelper address6;
    address6.SetBase("2001:db8::", Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces6 = address6.Assign(devices);

    UdpFlowHelper::Setup(nodes.Get(0), nodes.Get(1), interfaces.GetAddress(1), interfaces6.GetAddress(1), useIpv6, enableLogging);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}