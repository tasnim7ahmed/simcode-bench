#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETExample");

class BsmApplication : public Application
{
public:
    BsmApplication() {}
    virtual ~BsmApplication() {}

protected:
    virtual void StartApplication()
    {
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &BsmApplication::SendBsm, this);
    }

    virtual void StopApplication()
    {
        Simulator::Cancel(m_sendEvent);
    }

    void SendBsm()
    {
        NS_LOG_INFO("Sending BSM (Basic Safety Message)");
        // Here we could add code to actually send a packet, but for now, it's just a log message
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &BsmApplication::SendBsm, this);
    }

private:
    EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
    // Create nodes for the vehicles
    NodeContainer vehicleNodes;
    vehicleNodes.Create(5); // 5 vehicles

    // Set up the mobility model for vehicle movement
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    // Position vehicles along a straight road
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicleNodes);

    // Set constant velocity for vehicles (50 m/s)
    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mobilityModel = vehicleNodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mobilityModel->SetVelocity(Vector(50.0, 0.0, 0.0));
    }

    // Install Wave (802.11p) devices on vehicles
    YansWifiPhyHelper wavePhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper waveChannel = YansWifiChannelHelper::Default();
    wavePhy.SetChannel(waveChannel.Create());

    NqosWaveMacHelper waveMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper waveHelper = Wifi80211pHelper::Default();

    NetDeviceContainer vehicleDevices;
    vehicleDevices = waveHelper.Install(wavePhy, waveMac, vehicleNodes);

    // Install Internet stack on all vehicles
    InternetStackHelper internet;
    internet.Install(vehicleNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(vehicleDevices);

    // Install BSM application on vehicles
    for (uint32_t i = 0; i < vehicleNodes.GetN(); ++i)
    {
        Ptr<BsmApplication> app = CreateObject<BsmApplication>();
        vehicleNodes.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(10.0));
    }

    // Enable NetAnim tracing
    AnimationInterface anim("vanet_netanim.xml");
    anim.SetConstantPosition(vehicleNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(1), 50.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(2), 100.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(3), 150.0, 0.0);
    anim.SetConstantPosition(vehicleNodes.Get(4), 200.0, 0.0);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

