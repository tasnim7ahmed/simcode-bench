#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices;
    for (int i = 0; i < 4; ++i) {
        NetDeviceContainer link;
        if (i < 3) {
            link = pointToPoint.Install(nodes.Get(i), nodes.Get(i + 1));
        } else {
            link = pointToPoint.Install(nodes.Get(i), nodes.Get(0));
        }
        devices.Add(link.Get(0));
        devices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    for (int i = 0; i < 4; ++i) {
        UdpEchoServerHelper echoServer(port + i);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        int nextNode = (i + 1) % 4;
        UdpEchoClientHelper echoClient(interfaces.GetAddress(2 * nextNode), port + i);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}