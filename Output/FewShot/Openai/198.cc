#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;

void TxTrace(Ptr<const Packet> packet)
{
    g_packetsSent++;
}

void RxTrace(Ptr<const Packet> packet, const Address &address)
{
    g_packetsReceived++;
}

int main(int argc, char *argv[])
{
    // Logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(2);

    // Set up mobility for vehicles (optional, place vehicles at some distance)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(vehicles);

    // Install Wave/802.11p devices
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    WaveHelper waveHelper = WaveHelper::Default();

    NetDeviceContainer devices = waveHelper.Install(wavePhy, waveMac, vehicles);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server (vehicle 1)
    uint16_t port = 4000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client (vehicle 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(128));
    ApplicationContainer clientApp = echoClient.Install(vehicles.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Packet tracing for statistics
    devices.Get(0)->TraceConnectWithoutContext("PhyTxBegin", MakeCallback(&TxTrace));
    devices.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    std::cout << "Packets Sent: " << g_packetsSent << std::endl;
    std::cout << "Packets Received: " << g_packetsReceived << std::endl;

    Simulator::Destroy();
    return 0;
}