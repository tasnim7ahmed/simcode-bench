#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    sourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApp = sourceHelper.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}