#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    MeshHelper mesh;
    mesh.SetStandard(WIFI_PHY_STANDARD_80211s);
    mesh.SetOperatingMode(WIFI_PHY_STANDARD_80211s, WifiMeshHelper::HT_OPERATION);
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);

    NetDeviceContainer meshDevices = mesh.Install(meshNodes);

    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    uint16_t sinkPort = 8080;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(meshNodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), sinkPort));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mb/s")));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onOffHelper.Install(meshNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    clientApp = onOffHelper.Install(meshNodes.Get(1));
    clientApp.Start(Seconds(3.0));
    clientApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(10.0));

    for (uint32_t i = 0; i < meshDevices.GetN(); ++i)
    {
        Ptr<NetDevice> nd = meshDevices.Get(i);
        std::ostringstream oss;
        oss << "mesh-node-" << i << ".pcap";
        Ptr<PcapFile> pcap = Create<PcapFile>();
        pcap->Open(oss.str(), PcapFile::DLT_IEEE802_11_RADIO);
        nd->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&PcapFile::Write, pcap));
        nd->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&PcapFile::Write, pcap));
    }

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4GlobalRouting> globalRouting = Ipv4GlobalRouting::GetObject<Ipv4GlobalRouting>();
    if (globalRouting)
    {
        globalRouting->PrintRoutingTableAllAt(Seconds(10.0), std::cout);
    }

    std::cout << "Flow Monitor Statistics:" << std::endl;
    monitor->SerializeToXmlFile("mesh-simulation.flowmon", true, true);

    Simulator::Destroy();

    return 0;
}