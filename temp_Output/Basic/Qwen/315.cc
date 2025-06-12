#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("input-defaults.txt"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Load"));
    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    // Create UE and gNB nodes
    NodeContainer ueNodes;
    NodeContainer gnbNodes;
    ueNodes.Create(1);
    gnbNodes.Create(1);

    // Create the 5G NR helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Create and set up the spectrum
    BandwidthPartInfoPtrVector allBwps;

    // Create BWP for UEs
    auto bwpi = std::make_shared<BandwidthPartInfo>();
    bwpi->m_ccId = 0;
    bwpi->m_bwpId = 0;
    bwpi->m_numerology = 1;
    bwpi->m_band = 28e9; // FR1: 28 GHz
    bwpi->m_channelBandwidth = 100e6;
    allBwps.push_back(bwpi);

    // Install devices
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);

    // Set channel conditions
    nrHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer gnbIpIface;
    gnbIpIface = ipv4.Assign(gnbDevs);

    // Setup applications
    uint16_t port = 9;

    // UDP Server on UE
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on gNB
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(gnbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Enable PCAP tracing
    nrHelper->EnableTraces();
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("nr-5g-udp-example.tr");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}