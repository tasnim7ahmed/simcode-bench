#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    Names::Add("n0", nodes.Get(0));
    Names::Add("n1", nodes.Get(1));
    Names::Add("n2", nodes.Get(2));
    Names::Add("n3", nodes.Get(3));

    // Configure point-to-point links
    PointToPointHelper pointToPoint02;
    pointToPoint02.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint02.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper pointToPoint12 = pointToPoint02;

    PointToPointHelper pointToPoint32;
    pointToPoint32.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    pointToPoint32.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install NetDevices
    NetDeviceContainer devices02 = pointToPoint02.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = pointToPoint12.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices32 = pointToPoint32.Install(nodes.Get(3), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces32 = address.Assign(devices32);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // CBR/UDP flow n0 -> n3
    uint16_t port03 = 9;
    UdpServerHelper server03(port03);
    ApplicationContainer serverApps03 = server03.Install(nodes.Get(3));
    serverApps03.Start(Seconds(1.0));
    serverApps03.Stop(Seconds(10.0));

    UdpClientHelper client03(interfaces32.GetAddress(0), port03);
    client03.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client03.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client03.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps03 = client03.Install(nodes.Get(0));
    clientApps03.Start(Seconds(2.0));
    clientApps03.Stop(Seconds(10.0));

    // CBR/UDP flow n3 -> n1
    uint16_t port31 = 10;
    UdpServerHelper server31(port31);
    ApplicationContainer serverApps31 = server31.Install(nodes.Get(1));
    serverApps31.Start(Seconds(1.0));
    serverApps31.Stop(Seconds(10.0));

    UdpClientHelper client31(interfaces12.GetAddress(0), port31);
    client31.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client31.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
    client31.SetAttribute("PacketSize", UintegerValue(210));
    ApplicationContainer clientApps31 = client31.Install(nodes.Get(3));
    clientApps31.Start(Seconds(2.0));
    clientApps31.Stop(Seconds(10.0));

    // FTP/TCP flow n0 -> n3
    uint16_t port03_tcp = 21;
    BulkSendHelper ftpClient03(interfaces32.GetAddress(0), port03_tcp);
    ftpClient03.SetAttribute("SendSize", UintegerValue(1024));

    ApplicationContainer ftpClientApps03 = ftpClient03.Install(nodes.Get(0));
    ftpClientApps03.Start(Seconds(1.2));
    ftpClientApps03.Stop(Seconds(1.35));

    PacketSinkHelper ftpServer03(port03_tcp);
    ApplicationContainer ftpServerApps03 = ftpServer03.Install(nodes.Get(3));
    ftpServerApps03.Start(Seconds(1.0));
    ftpServerApps03.Stop(Seconds(10.0));

    // Error Models
    Ptr<RateErrorModel> emRate = CreateObject<RateErrorModel>();
    emRate->SetAttribute("ErrorRate", DoubleValue(0.001));
    emRate->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));

    Ptr<BurstErrorModel> emBurst = CreateObject<BurstErrorModel>();
    emBurst->SetAttribute("ErrorRate", DoubleValue(0.01));
    emBurst->SetAttribute("BurstSize", StringValue("1"));
    emBurst->SetAttribute("RandomVariable", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1.0]"));

    ListErrorModel listErrorModel;
    listErrorModel.SetDrop(11);
    listErrorModel.SetDrop(17);

    devices02.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emRate));
    devices12.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(emBurst));
    devices32.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(&listErrorModel));

    // Queue Tracing
    AsciiTraceHelper ascii;
    pointToPoint02.EnableAsciiAll(ascii.CreateFileStream("simple-error-model.tr"));

    // Packet Tracing
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}