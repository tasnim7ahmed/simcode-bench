#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;

    // Create nodes (vehicles)
    NodeContainer nodes;
    nodes.Create(2);

    // Setup mobility for vehicles
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Start and stop mobility
    for (NodeContainer::Iterator i = nodes.Begin(); i != nodes.End(); ++i) {
        Ptr<Node> node = *i;
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        Ptr<ConstantVelocityMobilityModel> cvmm = mobility->GetObject<ConstantVelocityMobilityModel>();
        cvmm->SetVelocity(Vector(20, 0, 0));  // Moving along X-axis at 20 m/s
    }

    // Install Wave MAC and PHY
    WaveMacHelper waveMac = WaveMacHelper::Default();
    YansWavePhyHelper wavePhy = YansWavePhyHelper::Default();
    NqosWaveMacHelper nqosWaveMac = NqosWaveMacHelper::Default();

    wavePhy.SetPcapDataLinkType(YansWavePhyHelper::DLT_IEEE802_11_OFC_PKT);
    NetDeviceContainer devices = wavePhy.Install(nodes);
    waveMac.SetType("ns3::OcbWifiMac",
                    "QosSupported", BooleanValue(false),
                    "ChannelCenterFrequency", UintegerValue(5860));
    waveMac.Install(nqosWaveMac, devices);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAssigner;
    ipAssigner.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAssigner.Assign(devices);

    // Setup UDP application
    uint16_t port = 8000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Setup packet tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("v2v-packet-trace.tr");
    devices.Get(0)->GetChannel()->AddPacketPrinter([stream](Ptr<const Packet> p) {
        *stream->GetStream() << p << std::endl;
    });

    // Setup statistics
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
    Config::Connect("/NodeList/1/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&PacketSink::TraceRxPacket, sink));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}