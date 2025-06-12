#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;
using namespace mmwave;

NS_LOG_COMPONENT_DEFINE("MmWave5GSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("MmWave5GSimulation", LOG_LEVEL_INFO);

    // Simulation duration
    double simTime = 10.0;
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("input-defaults.xml"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("None"));

    // Create nodes: 1 gNB and 2 UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    NodeContainer allNodes = NodeContainer(enbNodes, ueNodes);

    // Setup mobility for nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes); // gNB is stationary

    ObjectFactory positionAllocator = ObjectFactory("ns3::RandomRectanglePositionAllocator",
                                                    "X",StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                                    "Y",StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));

    mobility.SetPositionAllocator(positionAllocator.Create<PositionAllocator>());
    mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    // Install the mmWave device
    MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();
    NetDeviceContainer enbDevs = mmwaveHelper.InstallEnbDevice(enbNodes, allNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes, allNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIfs = ipv4.Assign(enbDevs);
    Ipv4InterfaceContainer ueIfs = ipv4.Assign(ueDevs);

    // Attach UEs to gNB
    mmwaveHelper.AttachToEnb(ueDevs, enbDevs.Get(0));

    // Setup applications
    uint16_t dlPort = 5001;

    // Server (UE 1) listens on port 5001
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer dlSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(0));
    dlSinkApp.Start(Seconds(0.0));
    dlSinkApp.Stop(Seconds(simTime));

    // Client (UE 2) sends packets every 10ms of size 1024 bytes
    OnOffHelper client("ns3::UdpSocketFactory", Address(InetSocketAddress(ueIfs.GetAddress(0), dlPort)));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.01]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1000Mb/s")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(0.1));
    clientApp.Stop(Seconds(simTime - 0.1));

    // Enable PCAP tracing
    mmwaveHelper.EnableTraces();
    mmwaveHelper.EnableDlPdcpTraces();

    // Enable ASCII trace
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mmwave-5g-simulation.tr");
    mmwaveHelper.EnableDlMacTraces(stream);
    mmwaveHelper.EnableUlMacTraces(stream);
    mmwaveHelper.EnableRlcTraces(stream);

    // Enable PCAP on devices
    mmwaveHelper.EnablePcapAll("mmwave-5g-simulation");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}