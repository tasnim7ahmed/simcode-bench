#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    double simTime = 10.0;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1400));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("UseBuildings", BooleanValue(false));
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(2);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 150, 0, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    mobility.Install(ueNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    Config::SetDefault("ns3::LteAmc::AmcModel", EnumValue(LteAmc::PiroEW2010));
    Config::SetDefault("ns3::LteAmc::Ber", DoubleValue(0.00005));
    Config::SetDefault("ns3::LteHandoverAlgorithm::HandoverManagementSapUser::ReportNeighborCellRsrp", BooleanValue(true));

    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;

    ApplicationContainer serverApps;
    UdpServerHelper dlPacketSink(dlPort);
    serverApps.Add(dlPacketSink.Install(enbNodes.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper dlClient(ueIpIfaces.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps;
    clientApps.Add(dlClient.Install(ueNodes.Get(0)));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    lteHelper->EnableTraces();
    epcHelper->EnableMmeTraces();
    epcHelper->EnableSgwTraces();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();
    lteHelper->EnablePdcpTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}