#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("NrGnbNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("NrUeNetDevice", LOG_LEVEL_INFO);

    // Create containers for gNB and UE nodes
    NodeContainer gnbNodes;
    NodeContainer ueNodes;

    gnbNodes.Create(1);
    ueNodes.Create(1);

    // Install mobility models: gNB is stationary, UE moves in a straight line
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);
    ueNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));

    // Install NR module
    NrHelper nrHelper;

    // Set spectrum model
    BandInfoList bands;
    BandInfo band;
    band.fl = 3600000000;  // Frequency in Hz (3.6 GHz)
    band.fh = 3800000000;
    band.frHigh = true;
    bands.push_back(band);

    nrHelper.SetChannelConditionModelType("ns3::ThreeGppUmiStreetCanyonChannelConditionModel");
    nrHelper.SetPathlossModelType("ns3::ThreeGppUmiPropagationLossModel");

    // Install the NR devices
    NetDeviceContainer gnbDevs = nrHelper.InstallGnbDevice(gnbNodes, bands);
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes, bands);

    // Install EPC to enable internet connectivity
    Ptr<Node> pgw = nrHelper.GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));

    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = nrHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the gNBs
    nrHelper.AttachToClosestGnb(ueDevs, gnbDevs);

    // Set up UDP server on the remote host (listening at port 9)
    uint16_t dlPort = 9;
    UdpServerHelper dlPacketSink(dlPort);
    ApplicationContainer dlAppContainer = dlPacketSink.Install(remoteHost);
    dlAppContainer.Start(Seconds(0.0));
    dlAppContainer.Stop(Seconds(10.0));

    // Set up UDP client on the UE to send packets every 500ms
    UdpClientHelper ulPacketGenerator(interfaces.GetAddress(1), dlPort);
    ulPacketGenerator.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite packets
    ulPacketGenerator.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    ulPacketGenerator.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer ulAppContainer = ulPacketGenerator.Install(ueNodes.Get(0));
    ulAppContainer.Start(Seconds(2.0));
    ulAppContainer.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}