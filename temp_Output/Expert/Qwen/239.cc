#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PNetwork");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("5Mbps"));
    Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("2ms"));

    NodeContainer clientNode;
    clientNode.Create(1);

    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    PointToPointHelper p2pClientRouter;
    NetDeviceContainer clientRouterDevices = p2pClientRouter.Install(clientNode.Get(0), routerNode.Get(0));

    PointToPointHelper p2pRouterServer;
    NetDeviceContainer routerServerDevices = p2pRouterServer.Install(routerNode.Get(0), serverNode.Get(0));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(routerServerInterfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(serverNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = clientHelper.Install(clientNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Tx Packets = " << iter->second.txPackets << " Rx Packets = " << iter->second.rxPackets);
    }

    Simulator::Destroy();
    return 0;
}