#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    Config::SetDefault("ns3::TcpBbr::MinRttWindowSize", TimeValue(Seconds(10)));
    Config::SetDefault("ns3::TcpBbr::ProbeRttDuration", TimeValue(MilliSeconds(200)));

    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer senderRouter1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer router1Router2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NodeContainer router2Receiver = NodeContainer(nodes.Get(2), nodes.Get(3));

    PointToPointHelper p2pHighBandwidth;
    p2pHighBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
    p2pHighBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    PointToPointHelper p2pLowBandwidth;
    p2pLowBandwidth.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2pLowBandwidth.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer devicesSenderRouter1;
    NetDeviceContainer devicesRouter1Router2;
    NetDeviceContainer devicesRouter2Receiver;

    devicesSenderRouter1 = p2pHighBandwidth.Install(senderRouter1);
    devicesRouter1Router2 = p2pLowBandwidth.Install(router1Router2);
    devicesRouter2Receiver = p2pHighBandwidth.Install(router2Receiver);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesSenderRouter1 = address.Assign(devicesSenderRouter1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesRouter1Router2 = address.Assign(devicesRouter1Router2);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesRouter2Receiver = address.Assign(devicesRouter2Receiver);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(100.0));

    InetSocketAddress receiverAddress(interfacesRouter2Receiver.GetAddress(1), sinkPort);
    receiverAddress.SetTos(0xb8);

    OnOffHelper client("ns3::TcpSocketFactory", receiverAddress);
    client.SetConstantRate(DataRate("1000Mbps"));
    client.SetAttribute("PacketSize", UintegerValue(1448));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.1));
    clientApp.Stop(Seconds(99.0));

    AsciiTraceHelper asciiTraceHelper;
    p2pLowBandwidth.EnablePcapAll("bbr_congestion", true, true);

    std::ofstream cwndStream;
    cwndStream.open("bbr_cwnd_trace.txt");

    std::ofstream queueSizeStream;
    queueSizeStream.open("queue_size_trace.txt");

    std::ofstream throughputStream;
    throughputStream.open("sender_throughput_trace.txt");

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    uint64_t lastTotalRx = 0;

    for (uint32_t i = 0; i < devicesRouter1Router2.GetN(); ++i) {
        Ptr<PointToPointCloudDevice> device = DynamicCast<PointToPointCloudDevice>(devicesRouter1Router2.Get(i));
        if (device) {
            device->GetQueue()->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&CwndChange, &cwndStream));
            device->GetQueue()->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&CwndChange, &cwndStream));
        }
    }

    Simulator::ScheduleNow(&TraceCwnd, &cwndStream);
    Simulator::ScheduleNow(&TraceThroughput, &throughputStream, sink, &lastTotalRx);
    Simulator::ScheduleNow(&TraceQueueSize, &queueSizeStream, devicesRouter1Router2);

    Simulator::Stop(Seconds(100));
    Simulator::Run();

    cwndStream.close();
    queueSizeStream.close();
    throughputStream.close();

    Simulator::Destroy();
    return 0;
}

void CwndChange(std::ostream *os, uint32_t oldVal, uint32_t newVal) {
    *os << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void TraceCwnd(std::ostream *os) {
    for (auto socket : Node::GetAllSockets()) {
        if (socket->GetNode()->GetId() == 0 && socket->GetInstanceTypeId().GetName() == "ns3::TcpSocketBase") {
            Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase>(socket);
            if (tcpSocket) {
                *os << Simulator::Now().GetSeconds() << " " << tcpSocket->GetCongestionWindow() << std::endl;
            }
        }
    }
    Simulator::Schedule(Seconds(0.1), &TraceCwnd, os);
}

void TraceThroughput(std::ostream *os, Ptr<PacketSink> sink, uint64_t *lastTotalRx) {
    double now = Simulator::Now().GetSeconds();
    uint64_t totalRx = sink->GetTotalRx();
    double delta = (totalRx - *lastTotalRx) * 8.0 / 1e6; // Mbps
    *os << now << " " << delta << std::endl;
    *lastTotalRx = totalRx;
    Simulator::Schedule(Seconds(0.5), &TraceThroughput, os, sink, lastTotalRx);
}

void TraceQueueSize(std::ostream *os, NetDeviceContainer devices) {
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<Queue<Packet>> queue = devices.Get(i)->GetObject<PointToPointCloudDevice>()->GetQueue();
        *os << Simulator::Now().GetSeconds() << " " << queue->GetNPackets() << std::endl;
    }
    Simulator::Schedule(Seconds(0.01), &TraceQueueSize, os, devices);
}