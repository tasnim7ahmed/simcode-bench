#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer n0, n1, n2, n3;
    n0.Create(1);
    n1.Create(1);
    n2.Create(1);
    n3.Create(1);

    // Create node containers for links
    NodeContainer p2p_n0n2 = NodeContainer(n0, n2);
    NodeContainer p2p_n1n2 = NodeContainer(n1, n2);
    NodeContainer p2p_n3n2 = NodeContainer(n3, n2);

    // Configure point-to-point links
    PointToPointHelper p2p_5mbps;
    p2p_5mbps.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_5mbps.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_1_5mbps;
    p2p_1_5mbps.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1_5mbps.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install net devices
    NetDeviceContainer d_n0n2 = p2p_5mbps.Install(p2p_n0n2);
    NetDeviceContainer d_n1n2 = p2p_5mbps.Install(p2p_n1n2);
    NetDeviceContainer d_n3n2 = p2p_1_5mbps.Install(p2p_n3n2);

    // Add error models
    Ptr<RateErrorModel> rem1 = CreateObject<RateErrorModel>();
    rem1->SetAttribute("ErrorRate", DoubleValue(0.001));
    d_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(rem1));

    Ptr<BurstErrorModel> bem = CreateObject<BurstErrorModel>();
    bem->SetAttribute("ErrorRate", DoubleValue(0.01));
    d_n3n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(bem));

    Ptr<ListErrorModel> lem = CreateObject<ListErrorModel>();
    std::list<uint64_t> uids = {11, 17};
    lem->SetList(uids);
    d_n1n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(lem));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_n0n2 = address.Assign(d_n0n2);

    address.NewNetwork();
    Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

    address.NewNetwork();
    Ipv4InterfaceContainer i_n3n2 = address.Assign(d_n3n2);

    // Set up UDP/CBR flows
    uint16_t udpPort = 9;

    // Flow from n0 to n3
    UdpClientHelper udpClient_n0(i_n3n2.GetAddress(0), udpPort);
    udpClient_n0.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient_n0.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    udpClient_n0.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer clientApp_n0 = udpClient_n0.Install(n0);
    clientApp_n0.Start(Seconds(0.0));
    clientApp_n0.Stop(Seconds(10.0));

    UdpServerHelper udpServer_n3;
    ApplicationContainer serverApp_n3 = udpServer_n3.Install(n3);
    serverApp_n3.Start(Seconds(0.0));
    serverApp_n3.Stop(Seconds(10.0));

    // Flow from n3 to n1
    UdpClientHelper udpClient_n3(i_n1n2.GetAddress(1), udpPort);
    udpClient_n3.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    udpClient_n3.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    udpClient_n3.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer clientApp_n3 = udpClient_n3.Install(n3);
    clientApp_n3.Start(Seconds(0.0));
    clientApp_n3.Stop(Seconds(10.0));

    UdpServerHelper udpServer_n1;
    ApplicationContainer serverApp_n1 = udpServer_n1.Install(n1);
    serverApp_n1.Start(Seconds(0.0));
    serverApp_n1.Stop(Seconds(10.0));

    // Set up FTP/TCP flow from n0 to n3
    uint16_t tcpPort = 50000;

    InetSocketAddress sinkAddr(i_n3n2.GetAddress(0), tcpPort);
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = packetSinkHelper.Install(n3);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onOffHelper("ns3::TcpSocketFactory", sinkAddr);
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("448Kbps")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp_tcp = onOffHelper.Install(n0);
    clientApp_tcp.Start(Seconds(1.2));
    clientApp_tcp.Stop(Seconds(1.35));

    // Setup PCAP tracing and ASCII trace
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simple-error-model.tr");

    p2p_5mbps.EnableAsciiAll(stream);
    p2p_1_5mbps.EnableAsciiAll(stream);

    p2p_5mbps.EnablePcapAll("simple-error-model");
    p2p_1_5mbps.EnablePcapAll("simple-error-model");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}