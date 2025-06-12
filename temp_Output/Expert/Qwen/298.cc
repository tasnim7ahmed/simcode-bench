#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverSimulation");

int main(int argc, char *argv[]) {
    uint16_t numberOfEnb = 2;
    uint16_t numberOfUe = 1;
    double simTime = 10.0;
    double distance = 60.0;
    double interEnbDistance = 100.0;

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRrcSap", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::Scheduler", StringValue("ns3::PfFfMacScheduler"));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnb);

    NodeContainer ueNodes;
    ueNodes.Create(numberOfUe);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(interEnbDistance), 
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-distance, interEnbDistance + distance, -50.0, 50.0)),
                              "Speed", StringValue("ns3::ConstantVariable[Value=3]"));
    mobility.Install(enbNodes);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-distance, interEnbDistance + distance, -50.0, 50.0)),
                              "Speed", StringValue("ns3::ConstantVariable[Value=10]"));
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    EpcTft tft;
    Ptr<EpcTft> tft1 = Create<EpcTft>();
    EpcTft::PacketFilter dlpf;
    dlpf.localPortStart = 8000;
    dlpf.localPortEnd = 8000;
    tft1->Add(dlpf);
    lteHelper->ActivateDedicatedEpsBearer(ueDevs, enbDevs, EpsBearer(EpsBearer::NGBR_IMS), tft1);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIfaces = ipv4h.Assign(ueDevs);
    Ipv4InterfaceContainer enbIpIfaces = ipv4h.Assign(enbDevs);

    uint16_t dlPort = 8000;
    uint16_t ulPort = 8001;
    uint16_t otherPort = 8002;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    UdpServerHelper dlServer(dlPort);
    serverApps.Add(dlServer.Install(enbNodes.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper dlClient(enbIpIfaces.GetAddress(0, 0), dlPort);
    dlClient.SetAttribute("MaxBytes", UintegerValue(0));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(dlClient.Install(ueNodes.Get(0)));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    lteHelper->EnableTraces();
    epcHelper->EnableMmeTraces();
    epcHelper->EnableSgwTraces();
    epcHelper->EnablePgwTraces();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();
    lteHelper->EnablePdcpTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}