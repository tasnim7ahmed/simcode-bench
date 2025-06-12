#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FourNodeSimulation");

int main(int argc, char *argv[]) {
    // Log components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer n0n2(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2(nodes.Get(1), nodes.Get(2));
    NodeContainer n3n2(nodes.Get(3), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Point-to-point links
    PointToPointHelper p2pHelper;

    // Link from n0 to n2
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2pHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices_n0n2 = p2pHelper.Install(n0n2);

    // Link from n1 to n2
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2pHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer devices_n1n2 = p2pHelper.Install(n1n2);

    // Link from n3 to n2
    p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
    p2pHelper.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer devices_n3n2 = p2pHelper.Install(n3n2);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_n0n2 = address.Assign(devices_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_n1n2 = address.Assign(devices_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces_n3n2 = address.Assign(devices_n3n2);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Error models
    Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel>();
    rateError->SetAttribute("ErrorRate", DoubleValue(0.001));

    Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel>();
    burstError->SetAttribute("ErrorRate", DoubleValue(0.01));

    Ptr<ListErrorModel> listError = CreateObject<ListErrorModel>();
    std::list<uint64_t> packetsToDrop;
    packetsToDrop.push_back(11);
    packetsToDrop.push_back(17);
    listError->SetAttribute("PacketList", ListValue(packetsToDrop));

    // Combine error models using CompoundErrorModel
    Ptr<CompoundErrorModel> compoundError = CreateObject<CompoundErrorModel>();
    compoundError->Add(rateError);
    compoundError->Add(burstError);
    compoundError->Add(listError);

    // Apply error model to the receiving device on n2 from n3
    devices_n3n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(compoundError));

    // Tracing queues and receptions
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simple-error-model.tr");

    // Enable queue tracing
    p2pHelper.EnableQueueTracesAll(stream);

    // Enable packet reception tracing (Pcap)
    p2pHelper.EnablePcapAll("simple-error-model");

    // CBR/UDP flow from n0 to n3
    uint16_t cbrPort = 9;
    UdpServerHelper server(cbrPort);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(2.0));

    UdpClientHelper client(interfaces_n3n2.GetAddress(0), cbrPort);
    client.SetAttribute("MaxBytes", UintegerValue(0));
    client.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.5));
    clientApp.Stop(Seconds(2.0));

    // FTP/TCP flow from n0 to n3 (BulkSend over TCP)
    uint16_t tcpPort = 50000;
    InetSocketAddress sinkAddress(interfaces_n3n2.GetAddress(0), tcpPort);

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(3));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(2.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer sourceApp = bulkSend.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.2));
    sourceApp.Stop(Seconds(1.35));

    // CBR/UDP flow from n3 to n1
    uint16_t cbrPort2 = 8;
    UdpServerHelper server2(interfaces_n1n2.GetAddress(1), cbrPort2);
    ApplicationContainer serverApp2 = server2.Install(nodes.Get(1));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(2.0));

    UdpClientHelper client2(interfaces_n1n2.GetAddress(1), cbrPort2);
    client2.SetAttribute("MaxBytes", UintegerValue(0));
    client2.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client2.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer clientApp2 = client2.Install(nodes.Get(3));
    clientApp2.Start(Seconds(0.5));
    clientApp2.Stop(Seconds(2.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}