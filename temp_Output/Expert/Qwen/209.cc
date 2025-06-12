#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t numClients = 5;
    Time simulationTime = Seconds(10.0);
    std::string protocol = "TcpSocketFactory";
    uint16_t port = 50000;
    uint32_t packetSize = 1024;
    DataRate dataRate("1Mbps");
    uint32_t mtu = 1500;
    bool enableFlowMonitor = true;
    std::string animFile = "star-topology.xml";

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", DoubleValue(0));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    NodeContainer clients;
    clients.Create(numClients);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

    NetDeviceContainer serverDevices;
    NetDeviceContainer clientDevices[numClients];

    for (uint32_t i = 0; i < numClients; ++i) {
        NetDeviceContainer link = p2p.Install(clients.Get(i), server.Get(0));
        clientDevices[i] = link.Get(0);
        serverDevices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer serverInterfaces;
    Ipv4InterfaceContainer clientInterfaces[numClients];

    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < numClients; ++i) {
        Ipv4InterfaceContainer ifaces = address.Assign(clientDevices[i]);
        clientInterfaces[i] = ifaces;
        serverInterfaces.Add(ifaces.Get(1));
        address.NewNetwork();
    }

    uint16_t sinkPort = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    PacketSinkHelper sinkHelper(protocol, sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(server.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simulationTime);

    OnOffHelper onoff(protocol, Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numClients; ++i) {
        AddressValue remoteAddress(InetSocketAddress(serverInterfaces.GetAddress(0), sinkPort));
        onoff.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onoff.Install(clients.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(simulationTime);
        clientApps.Add(app);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AnimationInterface anim(animFile);
    anim.SetMobilityPollInterval(Seconds(1));
    uint32_t nodeId = 0;
    anim.UpdateNodeDescription(server.Get(0), "Server");
    anim.UpdateNodeColor(server.Get(0), 255, 0, 0);
    for (auto &client : clients) {
        std::ostringstream oss;
        oss << "Client-" << nodeId++;
        anim.UpdateNodeDescription(client, oss.str());
        anim.UpdateNodeColor(client, 0, 0, 255);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    Simulator::Stop(simulationTime);
    Simulator::Run();

    if (enableFlowMonitor) {
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile("flowmonitor-results.xml", false, false);
    }

    Simulator::Destroy();

    return 0;
}