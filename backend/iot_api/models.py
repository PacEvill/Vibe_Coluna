from django.db import models

class SensorData(models.Model):
    timestamp = models.DateTimeField(auto_now_add=True)
    payload = models.JSONField(help_text="Data received from the wearable")

    def __str__(self):
        return f"Data at {self.timestamp}"
