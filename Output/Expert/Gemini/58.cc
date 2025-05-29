#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpBbr", LOG_LEVEL_ALL);
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer senderRouter;
    senderRouter.Add(nodes.Get(0));
    senderRouter.Add(nodes.Get(1));

    NodeContainer routerRouter;
    routerRouter.Add(nodes.Get(1));
    routerRouter.Add(nodes.Get(2));

    NodeContainer routerReceiver;
    routerReceiver.Add(nodes.Get(2));
    routerReceiver.Add(nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2pSenderRouter;
    p2pSenderRouter.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2pSenderRouter.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer senderRouterDevices = p2pSenderRouter.Install(senderRouter);

    PointToPointHelper p2pRouterRouter;
    p2pRouterRouter.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouterRouter.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer routerRouterDevices = p2pRouterRouter.Install(routerRouter);

    PointToPointHelper p2pRouterReceiver;
    p2pRouterReceiver.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2pRouterReceiver.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer routerReceiverDevices = p2pRouterReceiver.Install(routerReceiver);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderRouterInterfaces = address.Assign(senderRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerRouterInterfaces = address.Assign(routerRouterDevices);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer routerReceiverInterfaces = address.Assign(routerReceiverDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(routerReceiverInterfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(100.0));

    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(routerReceiverInterfaces.GetAddress(1), port));
    source.SetAttribute("SendSize", UintegerValue(1400));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(100.0));

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    p2pSenderRouter.EnablePcapAll("sender-router", false);
    p2pRouterRouter.EnablePcapAll("router-router", false);
    p2pRouterReceiver.EnablePcapAll("router-receiver", false);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(Seconds(10.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
    Simulator::Schedule(Seconds(20.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(30.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(40.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(50.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(60.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(70.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(80.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });
        Simulator::Schedule(Seconds(90.0), [](){
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpBbr"));
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/CongestionWindowProbeRtt", StringValue ("ns3::ConstantRandomVariable[Constant=4]"));
    });

    Simulator::Stop(Seconds(100.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::ofstream out("flowmon.xml");
    monitor->SerializeToXmlFile("flowmon.xml", false, true);

    Simulator::Destroy();
    return 0;
}