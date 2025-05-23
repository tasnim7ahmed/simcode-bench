#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    uint16_t serverPort = 5001;
    uint32_t packetSize = 1024;
    Time packetInterval = MilliSeconds(10);

    DataRate clientDataRate = DataRate((packetSize * 8) / packetInterval.GetSeconds());

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    Ptr<ConstantPositionMobilityModel> gnbMobility = CreateObject<ConstantPositionMobilityModel>();
    gnbMobility->SetPosition(Vector(25.0, 25.0, 0.0));
    gnbNodes.Get(0)->AggregateObject(gnbMobility);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Distance", StringValue("ns3::ConstantRandomVariable[Constant=5.0]"));
    mobility.Install(ueNodes);

    ns3::nr::NrHelper nrHelper;
    nrHelper.SetRrcProtocol("ns3::nr::RrcSapUser");
    nrHelper.SetMmWaveConfig();
    nrHelper.SetSchedulerType("ns3::nr::PFDScheduler");
    nrHelper.SetUlSchType("ns3::nr::PFDScheduler");

    NetDeviceContainer gnbDevices;
    NetDeviceContainer ueDevices;
    std::pair<NetDeviceContainer, NetDeviceContainer> devicePair = nrHelper.Install(gnbNodes, ueNodes);
    gnbDevices = devicePair.first;
    ueDevices = devicePair.second;

    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIfaces = ipv4.Assign(gnbDevices);
    Ipv4InterfaceContainer ueIpIfaces = ipv4.Assign(ueDevices);

    InetSocketAddress serverAddress(Ipv4Address::GetAny(), serverPort);
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", serverAddress);
    ApplicationContainer serverApps = sinkHelper.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    Ipv4Address ue1IpAddress = ueIpIfaces.GetAddress(0);
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ue1IpAddress, serverPort));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(clientDataRate));

    ApplicationContainer clientApps = onoff.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    nrHelper.EnablePcapAll("nr-mmwave-simulation", true);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}