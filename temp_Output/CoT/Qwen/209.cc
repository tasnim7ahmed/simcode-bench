#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t numClients = 5;
    double simulationTime = 10.0;
    std::string protocol = "TcpNewReno";
    bool enableFlowMonitor = true;
    std::string animFile = "star-topology.xml";

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(0)));
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(Seconds(0.1)));

    NodeContainer clients;
    clients.Create(numClients);

    NodeContainer server;
    server.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientDevices[numClients];
    NetDeviceContainer serverDevices;

    for (uint32_t i = 0; i < numClients; ++i) {
        NetDeviceContainer devices = p2p.Install(clients.Get(i), server.Get(0));
        clientDevices[i] = devices;
        serverDevices.Add(devices.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[numClients + 1];

    for (uint32_t i = 0; i < numClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(clientDevices[i]);
    }

    address.SetBase("192.168.1.0", "255.255.255.0");
    interfaces[numClients] = address.Assign(serverDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(server.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < numClients; ++i) {
        AddressValue remoteAddress(InetSocketAddress(interfaces[numClients].GetAddress(0), port));
        onoff.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = onoff.Install(clients.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime - 0.1));
        clientApps.Add(app);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    AnimationInterface anim(animFile);
    anim.SetConstantPosition(server.Get(0), 0, 0);
    for (uint32_t i = 0; i < numClients; ++i) {
        double angle = 2 * M_PI * i / numClients;
        double x = 10.0 * cos(angle);
        double y = 10.0 * sin(angle);
        anim.SetConstantPosition(clients.Get(i), x, y);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile("star-throughput.xml", true, true);
    }

    Simulator::Destroy();

    return 0;
}